// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CAPTURE_BROWSER_CONTENT_CAPTURE_CONSUMER_H_
#define COMPONENTS_CONTENT_CAPTURE_BROWSER_CONTENT_CAPTURE_CONSUMER_H_

#include <vector>

#include "components/content_capture/browser/content_capture_frame.h"

class GURL;

namespace content_capture {

// The interface for the embedder to get onscreen content.
//
// The embedder shall call OnscreenContentProvider::AddConsumer() to add
// itself as consumer.
//
//   OnscreenContentProvider* provider =
//      OnscreenContentProvider::FromWebContents(web_contents);
//   if (!provider)
//     provider = OnscreenContentProvider::Create(web_contents);
//   provider->AddConsumer(*this);
//
// The embedder might remove itself when the onscreen content is no longer
// needed.
//   // Keep the weak reference
//   onscreen_content_provider_ = provider->GetWeakPtr();
//
//   // Remove from the consumers
//   if (auto* provider = onscreen_content_provider_.get())
//     provider->RemoveConsumer(*this);
class ContentCaptureConsumer {
 public:
  virtual ~ContentCaptureConsumer() = default;

  // Invoked when the captured content |data| from the |parent_session| was
  // received.
  virtual void DidCaptureContent(const ContentCaptureSession& parent_session,
                                 const ContentCaptureFrame& data) = 0;
  // Invoked when the updated content |data| from the |parent_session| was
  // received.
  virtual void DidUpdateContent(const ContentCaptureSession& parent_session,
                                const ContentCaptureFrame& data) = 0;
  // Invoked when the list of content |ids| of the given |session| was removed.
  virtual void DidRemoveContent(const ContentCaptureSession& session,
                                const std::vector<int64_t>& ids) = 0;
  // Invoked when the given |session| was removed because
  // - the corresponding frame is deleted,
  // - or the corresponding WebContents is deleted.
  // - or the consumer removes itself from OnscreenContentProvider, only
  //   main session will be notified in this case.
  virtual void DidRemoveSession(const ContentCaptureSession& session) = 0;
  // Invoked when the given |main_frame|'s title updated.
  virtual void DidUpdateTitle(const ContentCaptureFrame& main_frame) = 0;
  // Invoked when the given |main_frame|'s favicon updated.
  virtual void DidUpdateFavicon(const ContentCaptureFrame& main_frame) = 0;

  // Return if the |url| shall be captured. Even return false, the content might
  // still be streamed because of the other consumers require it. Consumer can
  // ignore the content upon it arrives.
  virtual bool ShouldCapture(const GURL& url) = 0;
};

}  // namespace content_capture

#endif  // COMPONENTS_CONTENT_CAPTURE_BROWSER_CONTENT_CAPTURE_CONSUMER_H_
