// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CAPTURE_BROWSER_CONTENT_CAPTURE_RECEIVER_MANAGER_H_
#define COMPONENTS_CONTENT_CAPTURE_BROWSER_CONTENT_CAPTURE_RECEIVER_MANAGER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/supports_user_data.h"
#include "components/content_capture/common/content_capture.mojom.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}  // namespace content

namespace content_capture {

class ContentCaptureReceiver;

// This class has an instance per WebContents, it is the base class of
// ContentCaptureReceiverManager implementation which shall overrides the pure
// virtual methods to get the messages from each receivers, this class creates
// the ContentCaptureReceiver and associates it with RenderFrameHost, it also
// binds ContentCaptureReceiver with its peer ContentCaptureSender in renderer.
// The ContentSession here is used to specify which frame the message came from.
class ContentCaptureReceiverManager : public content::WebContentsObserver,
                                      public base::SupportsUserData::Data {
 public:
  ~ContentCaptureReceiverManager() override;
  static ContentCaptureReceiverManager* FromWebContents(
      content::WebContents* contents);

  // Binds the |request| with the |render_frame_host| associated
  // ContentCaptureReceiver.
  static void BindContentCaptureReceiver(
      mojo::PendingAssociatedReceiver<mojom::ContentCaptureReceiver>
          pending_receiver,
      content::RenderFrameHost* render_frame_host);

  // The methods called by ContentCaptureReceiver.
  void DidCaptureContent(ContentCaptureReceiver* content_capture_receiver,
                         const ContentCaptureData& data);
  void DidUpdateContent(ContentCaptureReceiver* content_capture_receiver,
                        const ContentCaptureData& data);
  void DidRemoveContent(ContentCaptureReceiver* content_capture_receiver,
                        const std::vector<int64_t>& data);
  void DidRemoveSession(ContentCaptureReceiver* content_capture_receiver);

  // content::WebContentsObserver:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;

  size_t GetFrameMapSizeForTesting() const { return frame_map_.size(); }

 protected:
  ContentCaptureReceiverManager(content::WebContents* web_contents);

  // Invoked when the captured content |data| from the |parent_session| was
  // received.
  virtual void DidCaptureContent(const ContentCaptureSession& parent_session,
                                 const ContentCaptureData& data) = 0;
  // Invoked when the updated content |data| from the |parent_session| was
  // received.
  virtual void DidUpdateContent(const ContentCaptureSession& parent_session,
                                const ContentCaptureData& data) = 0;
  // Invoked when the list of content |ids| of the given |session| was removed.
  virtual void DidRemoveContent(const ContentCaptureSession& session,
                                const std::vector<int64_t>& ids) = 0;
  // Invoked when the given |session| was removed.
  virtual void DidRemoveSession(const ContentCaptureSession& session) = 0;

  virtual bool ShouldCapture(const GURL& url) = 0;

  // Visible for testing.
  ContentCaptureReceiver* ContentCaptureReceiverForFrame(
      content::RenderFrameHost* render_frame_host) const;

 private:
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

  std::map<content::RenderFrameHost*, std::unique_ptr<ContentCaptureReceiver>>
      frame_map_;
};

}  // namespace content_capture

#endif  // COMPONENTS_CONTENT_CAPTURE_BROWSER_CONTENT_CAPTURE_RECEIVER_MANAGER_H_
