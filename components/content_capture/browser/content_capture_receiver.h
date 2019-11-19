// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CAPTURE_BROWSER_CONTENT_CAPTURE_RECEIVER_H_
#define COMPONENTS_CONTENT_CAPTURE_BROWSER_CONTENT_CAPTURE_RECEIVER_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "components/content_capture/common/content_capture.mojom.h"
#include "components/content_capture/common/content_capture_data.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace content {
class RenderFrameHost;
}

namespace content_capture {

// This class has an instance per RenderFrameHost, it receives messages from
// renderer and forward them to ContentCaptureReceiverManager for further
// processing.
class ContentCaptureReceiver : public mojom::ContentCaptureReceiver {
 public:
  static int64_t GetIdFrom(content::RenderFrameHost* rfh);
  explicit ContentCaptureReceiver(content::RenderFrameHost* rfh);
  ~ContentCaptureReceiver() override;

  // Binds to mojom.
  void BindPendingReceiver(
      mojo::PendingAssociatedReceiver<mojom::ContentCaptureReceiver>
          pending_receiver);

  // mojom::ContentCaptureReceiver
  void DidCaptureContent(const ContentCaptureData& data,
                         bool first_data) override;
  void DidUpdateContent(const ContentCaptureData& data) override;
  void DidRemoveContent(const std::vector<int64_t>& data) override;
  void StartCapture();
  void StopCapture();

  content::RenderFrameHost* rfh() const { return rfh_; }

  // Return ContentCaptureData of the associated frame.
  const ContentCaptureData& GetFrameContentCaptureData();
  const ContentCaptureData& GetFrameContentCaptureDataLastSeen() const {
    return frame_content_capture_data_;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(ContentCaptureReceiverTest, RenderFrameHostGone);

  const mojo::AssociatedRemote<mojom::ContentCaptureSender>&
  GetContentCaptureSender();

  mojo::AssociatedReceiver<mojom::ContentCaptureReceiver> receiver_{this};
  content::RenderFrameHost* rfh_;
  ContentCaptureData frame_content_capture_data_;

  // The content id of the associated frame, it is composed of RenderProcessHost
  // unique ID and frame routing ID, and is unique in a WebContents.
  // The ID is always generated in receiver because neither does the parent
  // frame always have content, nor is its content always captured before child
  // frame's; if the Id is generated in sender, the
  // ContentCaptureReceiverManager can't get parent frame id in both cases.
  int64_t id_;
  bool content_capture_enabled_ = false;
  mojo::AssociatedRemote<mojom::ContentCaptureSender> content_capture_sender_;
  DISALLOW_COPY_AND_ASSIGN(ContentCaptureReceiver);
};

}  // namespace content_capture

#endif  // COMPONENTS_CONTENT_CAPTURE_BROWSER_CONTENT_CAPTURE_RECEIVER_H_
