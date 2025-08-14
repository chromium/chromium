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
    const std::vector<std::string>& names)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<PageContentMetadataObserver>(*web_contents),
      names_(names) {
  web_contents->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [this](content::RenderFrameHost* rfh) { RenderFrameCreated(rfh); });
}

PageContentMetadataObserver::~PageContentMetadataObserver() = default;

void PageContentMetadataObserver::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  if (frame_meta_tags_observers_.contains(render_frame_host)) {
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

  auto new_observer = std::make_unique<FrameMetaTagsObserver>(
      this, render_frame_host, std::move(observer_receiver));
  frame_meta_tags_observers_[render_frame_host] = std::move(new_observer);
}

void PageContentMetadataObserver::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  frame_meta_tags_observers_.erase(render_frame_host);
  frame_metadata_cache_.erase(render_frame_host);
}

void PageContentMetadataObserver::DOMContentLoaded(
    content::RenderFrameHost* render_frame_host) {
  // This is needed for frames that are not live when the observer is created.
  RenderFrameCreated(render_frame_host);
}

void PageContentMetadataObserver::PrimaryPageChanged(content::Page& page) {
  frame_metadata_cache_.clear();
}

void PageContentMetadataObserver::OnMetaTagsChangedForFrame(
    content::RenderFrameHost* render_frame_host,
    std::vector<blink::mojom::MetaTagPtr> meta_tags) {
  if (meta_tags.empty()) {
    frame_metadata_cache_.erase(render_frame_host);
  } else {
    auto frame_metadata = blink::mojom::FrameMetadata::New();
    frame_metadata->url = GetURLForFrameMetadata(
        render_frame_host->GetLastCommittedURL(),
        render_frame_host->GetLastCommittedOrigin());
    frame_metadata->meta_tags = std::move(meta_tags);
    frame_metadata_cache_[render_frame_host] = std::move(frame_metadata);
  }

  if (!on_meta_tags_changed_callback_) {
    return;
  }

  auto page_metadata = blink::mojom::PageMetadata::New();

  for (const auto& [rfh, frame_metadata] : frame_metadata_cache_) {
    if (!rfh->GetParent()) {
      // The metadata for the main frame should be the first entry.
      page_metadata->frame_metadata.insert(
          page_metadata->frame_metadata.begin(), frame_metadata.Clone());
    } else {
      page_metadata->frame_metadata.push_back(frame_metadata.Clone());
    }
  }

  on_meta_tags_changed_callback_.Run(render_frame_host, *page_metadata);
}

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
    std::vector<blink::mojom::MetaTagPtr> meta_tags) {
  owner_->OnMetaTagsChangedForFrame(render_frame_host_, std::move(meta_tags));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PageContentMetadataObserver);

}  // namespace optimization_guide
