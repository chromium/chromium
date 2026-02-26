// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_METADATA_OBSERVER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_METADATA_OBSERVER_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/content/browser/media_transcript_provider.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/content_extraction/frame_metadata_observer_registry.mojom.h"
#include "third_party/blink/public/mojom/page/page.mojom-forward.h"

namespace content {
class NavigationHandle;
class Page;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace optimization_guide {

class PageContentMetadataObserver;

class MediaTranscriptObserver
    : public content::DocumentUserData<MediaTranscriptObserver> {
 public:
  ~MediaTranscriptObserver() override;

  // Called when transcription begins for the frame.
  void OnTranscriptionBegin(content::RenderFrameHost* rfh);

  void AddOwner(base::WeakPtr<PageContentMetadataObserver> owner);

  DOCUMENT_USER_DATA_KEY_DECL();

 protected:
  explicit MediaTranscriptObserver(content::RenderFrameHost* rfh);

 private:
  friend class content::DocumentUserData<MediaTranscriptObserver>;

  // Needs a list of owners to be able to support multiple instances of GiC.
  std::list<base::WeakPtr<PageContentMetadataObserver>> owners_;
};

// A class that is responsible for observing metadata for all frames in a
// WebContents. For each remote frame, it will register a MetaTagsObserver to
// receive metadata from the frame.
//
// This class currently only supports a single observer and set of names. Each
// consumer should create a separate instance of this class to observe metadata
// for a set of meta tags.
class PageContentMetadataObserver : public content::WebContentsObserver {
 public:
  using OnPageMetadataChangedCallback =
      base::RepeatingCallback<void(blink::mojom::PageMetadataPtr)>;

  PageContentMetadataObserver(content::WebContents* web_contents,
                              const std::vector<std::string>& names,
                              OnPageMetadataChangedCallback callback);
  ~PageContentMetadataObserver() override;
  PageContentMetadataObserver(const PageContentMetadataObserver&) = delete;
  PageContentMetadataObserver& operator=(const PageContentMetadataObserver&) =
      delete;

  // Delivers the current metadata to the callback.  Clients may use this to
  // prompt sending the most recent metadata.
  void DispatchMetadata();

  // Called when transcription begins for a frame.
  // Marked virtual for testing.
  virtual void OnTranscriptionBegin(content::RenderFrameHost* rfh);

 private:
  // content::WebContentsObserver:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void PrimaryPageChanged(content::Page& page) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  void OnMetaTagsChangedForFrame(
      content::RenderFrameHost* render_frame_host,
      std::vector<blink::mojom::MetaTagPtr> meta_tags);

  void UpdateFrameObservers();

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
        std::vector<::blink::mojom::MetaTagPtr> meta_tags) override;
    raw_ptr<PageContentMetadataObserver> owner_;
    raw_ptr<content::RenderFrameHost> render_frame_host_;
    mojo::Receiver<blink::mojom::MetaTagsObserver> receiver_;
  };

  struct FrameData {
    explicit FrameData(std::unique_ptr<FrameMetaTagsObserver> observer);
    ~FrameData();
    FrameData(FrameData&&);
    FrameData& operator=(FrameData&&);

    std::unique_ptr<FrameMetaTagsObserver> observer;
    blink::mojom::FrameMetadataPtr metadata;
  };

  bool UpdateMediaTranscriptsFlag(
      FrameData& curr_frame_metadata,
      content::RenderFrameHost* render_frame_host,
      std::vector<blink::mojom::MetaTagPtr>& meta_tags);

  const std::vector<std::string> names_;

  base::flat_map<content::RenderFrameHost*, FrameData> frame_data_;

  OnPageMetadataChangedCallback callback_;

  base::WeakPtrFactory<PageContentMetadataObserver> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_METADATA_OBSERVER_H_
