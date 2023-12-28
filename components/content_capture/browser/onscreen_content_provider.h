// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CAPTURE_BROWSER_ONSCREEN_CONTENT_PROVIDER_H_
#define COMPONENTS_CONTENT_CAPTURE_BROWSER_ONSCREEN_CONTENT_PROVIDER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/content_capture/browser/content_capture_frame.h"
#include "components/content_capture/common/content_capture.mojom.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
class NavigationEntry;
}  // namespace content

namespace content_capture {

class ContentCaptureReceiver;
class ContentCaptureConsumer;

// This class has an instance per WebContents, it is the base class of
// ContentCaptureReceiverManager implementation which shall overrides the pure
// virtual methods to get the messages from each receivers, this class creates
// the ContentCaptureReceiver and associates it with RenderFrameHost, it also
// binds ContentCaptureReceiver with its peer ContentCaptureSender in renderer.
// The ContentSession here is used to specify which frame the message came from.
class OnscreenContentProvider
    : public content::WebContentsObserver,
      public content::WebContentsUserData<OnscreenContentProvider> {
 public:
  ~OnscreenContentProvider() override;
  static OnscreenContentProvider* Create(content::WebContents* web_contents);

  // Binds the |request| with the |render_frame_host| associated
  // ContentCaptureReceiver.
  static void BindContentCaptureReceiver(
      mojo::PendingAssociatedReceiver<mojom::ContentCaptureReceiver>
          pending_receiver,
      content::RenderFrameHost* render_frame_host);

  void AddConsumer(ContentCaptureConsumer& consumer);
  void RemoveConsumer(ContentCaptureConsumer& consumer);

  // The methods called by ContentCaptureReceiver.
  void DidCaptureContent(ContentCaptureReceiver* content_capture_receiver,
                         const ContentCaptureFrame& data);
  void DidUpdateContent(ContentCaptureReceiver* content_capture_receiver,
                        const ContentCaptureFrame& data);
  void DidRemoveContent(ContentCaptureReceiver* content_capture_receiver,
                        const std::vector<int64_t>& data);
  void DidRemoveSession(ContentCaptureReceiver* content_capture_receiver);
  void DidUpdateTitle(ContentCaptureReceiver* content_capture_receiver);
  void DidUpdateFavicon(ContentCaptureReceiver* content_capture_receiver);

  // content::WebContentsObserver:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  void TitleWasSet(content::NavigationEntry* entry) override;
  void DidUpdateFaviconURL(
      content::RenderFrameHost* render_frame_host,
      const std::vector<blink::mojom::FaviconURLPtr>& candidates) override;

  size_t GetFrameMapSizeForTesting() const { return frame_map_.size(); }

  base::WeakPtr<OnscreenContentProvider> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void NotifyFaviconURLUpdatedForTesting(
      content::RenderFrameHost* render_frame_host,
      const std::vector<blink::mojom::FaviconURLPtr>& candidates) {
    NotifyFaviconURLUpdated(render_frame_host, candidates);
  }

#ifdef UNIT_TEST
  ContentCaptureReceiver* ContentCaptureReceiverForFrameForTesting(
      content::RenderFrameHost* render_frame_host) const {
    return ContentCaptureReceiverForFrame(render_frame_host);
  }

  const std::vector<raw_ptr<ContentCaptureConsumer, VectorExperimental>>&
  GetConsumersForTesting() const {
    return consumers_;
  }
#endif

 private:
  friend class content::WebContentsUserData<OnscreenContentProvider>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
  using content::WebContentsUserData<
      OnscreenContentProvider>::CreateForWebContents;

  explicit OnscreenContentProvider(content::WebContents* web_contents);

  ContentCaptureReceiver* ContentCaptureReceiverForFrame(
      content::RenderFrameHost* render_frame_host) const;

  // Builds ContentCaptureSession and returns in |session|, |ancestor_only|
  // specifies if only ancestor should be returned in |session|.
  void BuildContentCaptureSession(
      ContentCaptureReceiver* content_capture_receiver,
      bool ancestor_only,
      ContentCaptureSession* session);

  // Builds ContentCaptureSession for |content_capture_receiver| into |session|,
  // return true if succeed, this method returns the session that has been
  // reported and shall be used for removing session.
  bool BuildContentCaptureSessionLastSeen(
      ContentCaptureReceiver* content_capture_receiver,
      ContentCaptureSession* session);

  bool BuildContentCaptureSessionForMainFrame(ContentCaptureSession* session);

  bool ShouldCapture(const GURL& url);

  void NotifyFaviconURLUpdated(
      content::RenderFrameHost* render_frame_host,
      const std::vector<blink::mojom::FaviconURLPtr>& candidates);

  std::map<content::GlobalRenderFrameHostId,
           std::unique_ptr<ContentCaptureReceiver>>
      frame_map_;

  std::vector<raw_ptr<ContentCaptureConsumer, VectorExperimental>> consumers_;

  base::WeakPtrFactory<OnscreenContentProvider> weak_ptr_factory_{this};
};

}  // namespace content_capture

#endif  // COMPONENTS_CONTENT_CAPTURE_BROWSER_ONSCREEN_CONTENT_PROVIDER_H_
