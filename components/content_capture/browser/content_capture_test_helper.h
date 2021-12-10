// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CAPTURE_BROWSER_CONTENT_CAPTURE_TEST_HELPER_H_
#define COMPONENTS_CONTENT_CAPTURE_BROWSER_CONTENT_CAPTURE_TEST_HELPER_H_

#include "components/content_capture/browser/content_capture_consumer.h"
#include "components/content_capture/browser/content_capture_receiver.h"

namespace content_capture {

// Fake ContentCaptureSender to call ContentCaptureReceiver mojom interface.
class FakeContentCaptureSender {
 public:
  FakeContentCaptureSender();
  virtual ~FakeContentCaptureSender();

  void DidCaptureContent(const ContentCaptureData& captured_content,
                         bool first_data);

  void DidUpdateContent(const ContentCaptureData& captured_content);

  void DidRemoveContent(const std::vector<int64_t>& data);

  mojo::PendingAssociatedReceiver<mojom::ContentCaptureReceiver>
  GetPendingAssociatedReceiver();

 private:
  mojo::AssociatedRemote<mojom::ContentCaptureReceiver>
      content_capture_receiver_;
};

class SessionRemovedTestHelper {
 public:
  SessionRemovedTestHelper();
  virtual ~SessionRemovedTestHelper();

  void DidRemoveSession(const ContentCaptureSession& data);

  const std::vector<ContentCaptureSession>& removed_sessions() const {
    return removed_sessions_;
  }

  void Reset();

 private:
  std::vector<ContentCaptureSession> removed_sessions_;
};

// The helper class implements ContentCaptureConsumer and keeps the
// result for verification.
class ContentCaptureConsumerHelper : public ContentCaptureConsumer {
 public:
  explicit ContentCaptureConsumerHelper(
      SessionRemovedTestHelper* session_removed_test_helper);

  ~ContentCaptureConsumerHelper() override;

  // ContentCaptureConsumer
  void DidCaptureContent(const ContentCaptureSession& parent_session,
                         const ContentCaptureFrame& data) override;

  void DidUpdateContent(const ContentCaptureSession& parent_session,
                        const ContentCaptureFrame& data) override;

  void DidRemoveContent(const ContentCaptureSession& session,
                        const std::vector<int64_t>& ids) override;

  void DidRemoveSession(const ContentCaptureSession& data) override;

  void DidUpdateTitle(const ContentCaptureFrame& main_frame) override;

  void DidUpdateFavicon(const ContentCaptureFrame& main_frame) override;

  bool ShouldCapture(const GURL& url) override;

  const ContentCaptureSession& parent_session() const {
    return parent_session_;
  }

  const ContentCaptureSession& updated_parent_session() const {
    return updated_parent_session_;
  }

  const ContentCaptureSession& session() const { return session_; }

  const ContentCaptureFrame& captured_data() const { return captured_data_; }

  const ContentCaptureFrame& updated_data() const { return updated_data_; }

  const std::vector<ContentCaptureSession>& removed_sessions() const {
    return removed_sessions_;
  }

  const std::vector<int64_t>& removed_ids() const { return removed_ids_; }
  const std::u16string& updated_title() const { return updated_title_; }

  void Reset();

 private:
  ContentCaptureSession parent_session_;
  ContentCaptureSession updated_parent_session_;
  ContentCaptureSession session_;
  ContentCaptureFrame captured_data_;
  ContentCaptureFrame updated_data_;
  std::vector<int64_t> removed_ids_;
  std::vector<ContentCaptureSession> removed_sessions_;
  raw_ptr<SessionRemovedTestHelper> session_removed_test_helper_;
  std::u16string updated_title_;
};

}  // namespace content_capture

#endif  // COMPONENTS_CONTENT_CAPTURE_BROWSER_CONTENT_CAPTURE_TEST_HELPER_H_
