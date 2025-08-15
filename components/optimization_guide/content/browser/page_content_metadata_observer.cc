// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_metadata_observer.h"

#include "components/optimization_guide/content/browser/page_content_proto_util.h"
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

void PageContentMetadataObserver::UpdateFrameObservers() {
  web_contents()->ForEachRenderFrameHost([this](content::RenderFrameHost* rfh) {
    if (rfh->IsRenderFrameLive()) {
      // RenderFrameCreated has the logic to create the observer and add it
      // to the map if it doesn't exist.
      RenderFrameCreated(rfh);
    }
  });
}

void PageContentMetadataObserver::OnMetaTagsChangedForFrame(
    content::RenderFrameHost* render_frame_host,
    std::vector<blink::mojom::MetaTagPtr> meta_tags) {
  if (render_frame_host) {
    auto it = frame_data_.find(render_frame_host);
    if (it != frame_data_.end()) {
      if (meta_tags.empty()) {
        it->second.metadata.reset();
      } else {
        auto frame_metadata = blink::mojom::FrameMetadata::New();
        frame_metadata->url = GetURLForFrameMetadata(
            render_frame_host->GetLastCommittedURL(),
            render_frame_host->GetLastCommittedOrigin());
        frame_metadata->meta_tags = std::move(meta_tags);
        it->second.metadata = std::move(frame_metadata);
      }
    }
  }

  // The callback should always be valid, as it is a required parameter for
  // creation.
  DCHECK(callback_);

  auto page_metadata = blink::mojom::PageMetadata::New();

  for (const auto& [rfh, data] : frame_data_) {
    if (!data.metadata) {
      continue;
    }

    if (!rfh->GetParent()) {
      // The metadata for the main frame should be the first entry.
      page_metadata->frame_metadata.insert(
          page_metadata->frame_metadata.begin(), data.metadata.Clone());
    } else {
      page_metadata->frame_metadata.push_back(data.metadata.Clone());
    }
  }

  callback_.Run(*page_metadata);
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
