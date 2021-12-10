// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_capture/browser/content_capture_test_helper.h"

namespace content_capture {

FakeContentCaptureSender::FakeContentCaptureSender() = default;

FakeContentCaptureSender::~FakeContentCaptureSender() = default;

void FakeContentCaptureSender::DidCaptureContent(
    const ContentCaptureData& captured_content,
    bool first_data) {
  content_capture_receiver_->DidCaptureContent(captured_content, first_data);
}

void FakeContentCaptureSender::DidUpdateContent(
    const ContentCaptureData& captured_content) {
  content_capture_receiver_->DidUpdateContent(captured_content);
}

void FakeContentCaptureSender::DidRemoveContent(
    const std::vector<int64_t>& data) {
  content_capture_receiver_->DidRemoveContent(data);
}

mojo::PendingAssociatedReceiver<mojom::ContentCaptureReceiver>
FakeContentCaptureSender::GetPendingAssociatedReceiver() {
  return content_capture_receiver_.BindNewEndpointAndPassDedicatedReceiver();
}

SessionRemovedTestHelper::SessionRemovedTestHelper() = default;

SessionRemovedTestHelper::~SessionRemovedTestHelper() = default;

void SessionRemovedTestHelper::DidRemoveSession(
    const ContentCaptureSession& data) {
  removed_sessions_.push_back(data);
}

void SessionRemovedTestHelper::Reset() {
  removed_sessions_.clear();
}

ContentCaptureConsumerHelper::ContentCaptureConsumerHelper(
    SessionRemovedTestHelper* session_removed_test_helper)
    : session_removed_test_helper_(session_removed_test_helper) {}

ContentCaptureConsumerHelper::~ContentCaptureConsumerHelper() = default;

void ContentCaptureConsumerHelper::DidCaptureContent(
    const ContentCaptureSession& parent_session,
    const ContentCaptureFrame& data) {
  parent_session_ = parent_session;
  captured_data_ = data;
}

void ContentCaptureConsumerHelper::DidUpdateContent(
    const ContentCaptureSession& parent_session,
    const ContentCaptureFrame& data) {
  updated_parent_session_ = parent_session;
  updated_data_ = data;
}

void ContentCaptureConsumerHelper::DidRemoveContent(
    const ContentCaptureSession& session,
    const std::vector<int64_t>& ids) {
  session_ = session;
  removed_ids_ = ids;
}

void ContentCaptureConsumerHelper::DidRemoveSession(
    const ContentCaptureSession& data) {
  if (session_removed_test_helper_)
    session_removed_test_helper_->DidRemoveSession(data);
  removed_sessions_.push_back(data);
}

void ContentCaptureConsumerHelper::DidUpdateTitle(
    const ContentCaptureFrame& main_frame) {
  updated_title_ = main_frame.title;
}

void ContentCaptureConsumerHelper::DidUpdateFavicon(
    const ContentCaptureFrame& main_frame) {}

bool ContentCaptureConsumerHelper::ShouldCapture(const GURL& url) {
  return false;
}

void ContentCaptureConsumerHelper::Reset() {
  removed_sessions_.clear();
}

}  // namespace content_capture
