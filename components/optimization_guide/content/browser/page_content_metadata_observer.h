// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_METADATA_OBSERVER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_METADATA_OBSERVER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/content_extraction/frame_metadata_observer_registry.mojom.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace optimization_guide {

// A class that is responsible for observing metadata for all frames in a
// WebContents. For each remote frame, it will register a MetaTagsObserver to
// receive metadata from the frame.
class PageContentMetadataObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PageContentMetadataObserver> {
 public:
  ~PageContentMetadataObserver() override;

  PageContentMetadataObserver(const PageContentMetadataObserver&) = delete;
  PageContentMetadataObserver& operator=(const PageContentMetadataObserver&) =
      delete;

  using OnMetaTagsChangedCallback =
      base::RepeatingCallback<void(content::RenderFrameHost*,
                                   const blink::mojom::PageMetadata&)>;
  void SetOnMetaTagsChangedCallback(OnMetaTagsChangedCallback callback) {
    on_meta_tags_changed_callback_ = std::move(callback);
  }

  // content::WebContentsObserver:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void PrimaryPageChanged(content::Page& page) override;
  void DOMContentLoaded(content::RenderFrameHost* render_frame_host) override;

 private:
  friend class content::WebContentsUserData<PageContentMetadataObserver>;

  PageContentMetadataObserver(content::WebContents* web_contents,
                              const std::vector<std::string>& names);

  void OnMetaTagsChangedForFrame(content::RenderFrameHost* render_frame_host,
                                 std::vector<blink::mojom::MetaTagPtr>
                                     meta_tags);

  // The implementation of MetaTagsObserver that will be registered with each
  // frame.
  class FrameMetaTagsObserver : public blink::mojom::MetaTagsObserver {
   public:
    FrameMetaTagsObserver(
        PageContentMetadataObserver* owner,
        content::RenderFrameHost* render_frame_host,
        mojo::PendingReceiver<blink::mojom::MetaTagsObserver> receiver);
    ~FrameMetaTagsObserver() override;

    // blink::mojom::MetaTagsObserver:
    void OnMetaTagsChanged(
        std::vector<blink::mojom::MetaTagPtr> meta_tags) override;
    raw_ptr<PageContentMetadataObserver> owner_;
    raw_ptr<content::RenderFrameHost> render_frame_host_;
    mojo::Receiver<blink::mojom::MetaTagsObserver> receiver_;
  };

  const std::vector<std::string> names_;

  base::flat_map<content::RenderFrameHost*,
                 std::unique_ptr<FrameMetaTagsObserver>>
      frame_meta_tags_observers_;

  std::map<content::RenderFrameHost*, blink::mojom::FrameMetadataPtr>
      frame_metadata_cache_;

  OnMetaTagsChangedCallback on_meta_tags_changed_callback_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_METADATA_OBSERVER_H_
