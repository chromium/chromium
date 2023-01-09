// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CAPTURE_BROWSER_CONTENT_CAPTURE_RECEIVER_H_
#define COMPONENTS_CONTENT_CAPTURE_BROWSER_CONTENT_CAPTURE_RECEIVER_H_

#include <vector>

#include "base/cancelable_callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "components/content_capture/browser/content_capture_frame.h"
#include "components/content_capture/common/content_capture.mojom.h"
#include "components/content_capture/common/content_capture_data.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom-forward.h"

namespace content {
class RenderFrameHost;
}

namespace content_capture {

// This class has an instance per RenderFrameHost, it receives messages from
// renderer and forward them to OnscreenContentProvider for further
// processing.
class ContentCaptureReceiver : public mojom::ContentCaptureReceiver {
 public:
  static int64_t GetIdFrom(content::RenderFrameHost* rfh);
  explicit ContentCaptureReceiver(content::RenderFrameHost* rfh);

  ContentCaptureReceiver(const ContentCaptureReceiver&) = delete;
  ContentCaptureReceiver& operator=(const ContentCaptureReceiver&) = delete;

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

  // Return ContentCaptureFrame of the associated frame.
  const ContentCaptureFrame& GetContentCaptureFrame();
  const ContentCaptureFrame& GetContentCaptureFrameLastSeen() const {
    return frame_content_capture_data_;
  }

  void RemoveSession();

  void SetTitle(const std::u16string& title);
  void UpdateFaviconURL(
      const std::vector<blink::mojom::FaviconURLPtr>& candidates);

  static void DisableGetFaviconFromWebContentsForTesting();
  static bool disable_get_favicon_from_web_contents_for_testing();

 private:
  FRIEND_TEST_ALL_PREFIXES(ContentCaptureReceiverTest, RenderFrameHostGone);
  FRIEND_TEST_ALL_PREFIXES(ContentCaptureReceiverTest, TitleUpdateTaskDelay);
  FRIEND_TEST_ALL_PREFIXES(ContentCaptureReceiverTest, ConvertFaviconURLToJSON);

  static std::string ToJSON(
      const std::vector<blink::mojom::FaviconURLPtr>& candidates);

  // Retrieve favicon url from WebContents, the result is set to
  // |frame_content_capture_data_|.favicon.
  void RetrieveFaviconURL();

  void NotifyTitleUpdate();

  const mojo::AssociatedRemote<mojom::ContentCaptureSender>&
  GetContentCaptureSender();

  mojo::AssociatedReceiver<mojom::ContentCaptureReceiver> receiver_{this};
  raw_ptr<content::RenderFrameHost> rfh_;
  ContentCaptureFrame frame_content_capture_data_;

  // The content id of the associated frame, it is composed of RenderProcessHost
  // unique ID and frame routing ID, and is unique in a WebContents.
  // The ID is always generated in receiver because neither does the parent
  // frame always have content, nor is its content always captured before child
  // frame's; if the Id is generated in sender, the
  // OnscreenContentProvider can't get parent frame id in both cases.
  int64_t id_;
  bool content_capture_enabled_ = false;

  // Indicates whether this receiver is visible to consumer. It should be set
  // upon the |frame_content_capture_data_| is created and reset on the session
  // removed; the former is caused by either the content captured or the
  // |frame_content_capture_data_| required by child frame.
  bool has_session_ = false;

  // The TaskRunner for |notify_title_update_callback_| task. It is also used by
  // test to replace with TestMockTimeTaskRunner.
  scoped_refptr<base::SingleThreadTaskRunner> title_update_task_runner_;
  // Hold the task for cancelling on session end.
  std::unique_ptr<base::CancelableOnceClosure> notify_title_update_callback_;
  // The delay of |notify_title_update_callback_|, is increased exponentially to
  // prevent running frequently.
  unsigned exponential_delay_ = 1;

  static bool disable_get_favicon_from_web_contents_for_testing_;

  mojo::AssociatedRemote<mojom::ContentCaptureSender> content_capture_sender_;
};

}  // namespace content_capture

#endif  // COMPONENTS_CONTENT_CAPTURE_BROWSER_CONTENT_CAPTURE_RECEIVER_H_
