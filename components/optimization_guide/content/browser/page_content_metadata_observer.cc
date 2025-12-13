// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_metadata_observer.h"

#include "components/optimization_guide/content/browser/page_content_proto_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content_metadata.mojom.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/opengraph/metadata.mojom.h"

namespace optimization_guide {

PageContentMetadataObserver::PageContentMetadataObserver(
    content::WebContents* web_contents,
    const std::vector<std::string>& names,
    OnPageMetadataChangedCallback callback)
    : content::WebContentsObserver(web_contents),
      names_(names),
      callback_(std::move(callback)) {
  DCHECK(!names_.empty());
  UpdateFrameObservers();
}

PageContentMetadataObserver::~PageContentMetadataObserver() = default;

void PageContentMetadataObserver::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  if (&render_frame_host->GetPage() != &web_contents()->GetPrimaryPage()) {
    return;
  }

  if (frame_data_.contains(render_frame_host)) {
    return;
  }

  if (!render_frame_host->IsRenderFrameLive()) {
    return;
  }

  mojo::Remote<blink::mojom::FrameMetadataObserverRegistry> registry;
  render_frame_host->GetRemoteInterfaces()->GetInterface(
      registry.BindNewPipeAndPassReceiver());

  mojo::PendingRemote<blink::mojom::MetaTagsObserver> observer_remote;
  auto observer_receiver = observer_remote.InitWithNewPipeAndPassReceiver();

  // The renderer will use this remote to call the browser.
  registry->AddMetaTagsObserver(names_, std::move(observer_remote));

  frame_data_.try_emplace(
      render_frame_host,
      std::make_unique<FrameMetaTagsObserver>(this, render_frame_host,
                                              std::move(observer_receiver)));
}

void PageContentMetadataObserver::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  // A frame was removed, so we should notify the observer that the page
  // metadata has changed if it had metadata.
  auto it = frame_data_.find(render_frame_host);
  if (it == frame_data_.end()) {
    return;
  }

  bool had_metadata = !!it->second.metadata;
  frame_data_.erase(it);

  if (web_contents()->IsBeingDestroyed()) {
    // No need to notify of changes if the web contents is going away.
    return;
  }

  if (had_metadata) {
    OnMetaTagsChangedForFrame(nullptr, {});
  }
}

void PageContentMetadataObserver::PrimaryPageChanged(content::Page& page) {
  // The primary page has changed, so we need to reset all frame observers
  // and re-initialize them for the new page's frame tree.
  frame_data_.clear();
  UpdateFrameObservers();
  // The meta tags are guaranteed to be empty here but we dispatch an update
  // for all frames to ensure that observers are notified that any
  // previous tags are no longer found (because of the navigation).
  DispatchMetadata();
}

void PageContentMetadataObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  content::RenderFrameHost* render_frame_host =
      navigation_handle->GetRenderFrameHost();

  // PrimaryPageChanged handles main frame navigations. We only care about
  // subframes here.
  if (!render_frame_host->GetParent()) {
    return;
  }

  // Only handle navigations in the primary page.
  if (&render_frame_host->GetPage() != &web_contents()->GetPrimaryPage()) {
    return;
  }

  // A navigation has committed in a subframe, so the document in the frame has
  // changed. We need to tear down the old observer and create a new one.
  // We don't call RenderFrameDeleted() since that is for when the frame is
  // actually removed. In this case, the frame persists and is reused for a new
  // document.
  frame_data_.erase(render_frame_host);
  RenderFrameCreated(render_frame_host);
}

void PageContentMetadataObserver::UpdateFrameObservers() {
  auto* primary_main_frame = web_contents()->GetPrimaryMainFrame();
  if (!primary_main_frame) {
    return;
  }
  primary_main_frame->ForEachRenderFrameHost(
      [this](content::RenderFrameHost* rfh) {
        if (rfh->IsRenderFrameLive()) {
          // RenderFrameCreated has the logic to create the observer and add it
          // to the map if it doesn't exist.
          RenderFrameCreated(rfh);
        }
      });
}

void PageContentMetadataObserver::DispatchMetadata() {
  auto page_metadata = blink::mojom::PageMetadata::New();
  for (const auto& [render_frame_host, frame_data] : frame_data_) {
    if (frame_data.metadata) {
      page_metadata->frame_metadata.push_back(frame_data.metadata->Clone());
    } else {
      // Create a representation for a frame with no matching meta tags.
      auto frame_metadata = blink::mojom::FrameMetadata::New();
      frame_metadata->url =
          GetURLForFrameMetadata(render_frame_host->GetLastCommittedURL(),
                                 render_frame_host->GetLastCommittedOrigin());
      if (frame_metadata->url.is_empty()) {
        continue;
      }
      page_metadata->frame_metadata.push_back(std::move(frame_metadata));
    }
  }
  callback_.Run(std::move(page_metadata));
}

void PageContentMetadataObserver::OnMetaTagsChangedForFrame(
    content::RenderFrameHost* render_frame_host,
    std::vector<blink::mojom::MetaTagPtr> meta_tags) {
  // `render_frame_host` can be null when a frame is deleted, when this happens
  // do not update the frame data, but do notify the callback.
  if (render_frame_host) {
    DCHECK(render_frame_host->GetPage().IsPrimary());
    auto it = frame_data_.find(render_frame_host);
    if (it != frame_data_.end()) {
      if (meta_tags.empty()) {
        it->second.metadata.reset();
      } else {
        auto frame_metadata = blink::mojom::FrameMetadata::New();
        frame_metadata->url =
            GetURLForFrameMetadata(render_frame_host->GetLastCommittedURL(),
                                   render_frame_host->GetLastCommittedOrigin());
        frame_metadata->meta_tags = std::move(meta_tags);
        it->second.metadata = std::move(frame_metadata);
      }
    }
  }

  // The callback should always be valid, as it is a required parameter for
  // creation.
  DCHECK(callback_);

  DispatchMetadata();
}

PageContentMetadataObserver::FrameData::FrameData(
    std::unique_ptr<FrameMetaTagsObserver> observer)
    : observer(std::move(observer)) {}

PageContentMetadataObserver::FrameData::~FrameData() = default;

PageContentMetadataObserver::FrameData::FrameData(FrameData&&) = default;

PageContentMetadataObserver::FrameData&
PageContentMetadataObserver::FrameData::operator=(FrameData&&) = default;

PageContentMetadataObserver::FrameMetaTagsObserver::FrameMetaTagsObserver(
    PageContentMetadataObserver* owner,
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::MetaTagsObserver> receiver)
    : owner_(owner),
      render_frame_host_(render_frame_host),
      receiver_(this, std::move(receiver)) {}

PageContentMetadataObserver::FrameMetaTagsObserver::~FrameMetaTagsObserver() =
    default;

void PageContentMetadataObserver::FrameMetaTagsObserver::OnMetaTagsChanged(
    std::vector<::blink::mojom::MetaTagPtr> meta_tags) {
  owner_->OnMetaTagsChangedForFrame(render_frame_host_, std::move(meta_tags));
}

}  // namespace optimization_guide
