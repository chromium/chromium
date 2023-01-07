// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_capture/browser/content_capture_test_helper.h"

#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content_capture {

FakeContentCaptureSender::FakeContentCaptureSender() = default;

FakeContentCaptureSender::~FakeContentCaptureSender() = default;

void FakeContentCaptureSender::Bind(content::RenderFrameHost* frame) {
  DCHECK(frame);
  content_capture_receiver_.reset();
  OnscreenContentProvider::BindContentCaptureReceiver(
      content_capture_receiver_.BindNewEndpointAndPassDedicatedReceiver(),
      frame);
}

void FakeContentCaptureSender::DidCaptureContent(
    const ContentCaptureData& captured_content,
    bool first_data) {
  base::RunLoop run_loop;
  content_capture_receiver_->DidCaptureContent(captured_content, first_data);
  run_loop.RunUntilIdle();
}

void FakeContentCaptureSender::DidUpdateContent(
    const ContentCaptureData& captured_content) {
  base::RunLoop run_loop;
  content_capture_receiver_->DidUpdateContent(captured_content);
  run_loop.RunUntilIdle();
}

void FakeContentCaptureSender::DidRemoveContent(
    const std::vector<int64_t>& data) {
  base::RunLoop run_loop;
  content_capture_receiver_->DidRemoveContent(data);
  run_loop.RunUntilIdle();
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

ContentCaptureTestHelper::ContentCaptureTestHelper() = default;

ContentCaptureTestHelper::~ContentCaptureTestHelper() = default;

void ContentCaptureTestHelper::CreateProviderAndConsumer(
    content::WebContents* web_contents,
    SessionRemovedTestHelper* session_removed_helper) {
  onscreen_content_provider_ = OnscreenContentProvider::Create(web_contents);

  content_capture_consumer_ =
      std::make_unique<ContentCaptureConsumerHelper>(session_removed_helper);
  onscreen_content_provider_->AddConsumer(*(content_capture_consumer_.get()));
}

void ContentCaptureTestHelper::InitTestData(const std::u16string& data1,
                                            const std::u16string& data2) {
  ContentCaptureData child;
  // Have the unique id for text content.
  child.id = 2;
  child.value = u"Hello";
  child.bounds = gfx::Rect(5, 5, 5, 5);
  // No need to set id in sender.
  test_data_.value = data1;
  test_data_.bounds = gfx::Rect(10, 10);
  test_data_.children.push_back(child);
  test_data2_.value = data2;
  test_data2_.bounds = gfx::Rect(10, 10);
  test_data2_.children.push_back(child);

  ContentCaptureData child_change;
  // Same ID with child.
  child_change.id = 2;
  child_change.value = u"Hello World";
  child_change.bounds = gfx::Rect(5, 5, 5, 5);
  test_data_change_.value = data1;
  test_data_change_.bounds = gfx::Rect(10, 10);
  test_data_change_.children.push_back(child_change);

  // Update to test_data_.
  ContentCaptureData child2;
  // Have the unique id for text content.
  child2.id = 3;
  child2.value = u"World";
  child2.bounds = gfx::Rect(5, 10, 5, 5);
  test_data_update_.value = data1;
  test_data_update_.bounds = gfx::Rect(10, 10);
  test_data_update_.children.push_back(child2);
}

void VerifySession(const ContentCaptureSession& expected,
                   const ContentCaptureSession& result) {
  EXPECT_EQ(expected.size(), result.size());
  for (size_t i = 0; i < expected.size(); i++) {
    EXPECT_EQ(expected[i].id, result[i].id);
    EXPECT_EQ(expected[i].url, result[i].url);
    EXPECT_EQ(expected[i].bounds, result[i].bounds);
    EXPECT_TRUE(result[i].children.empty());
  }
}

ContentCaptureFrame GetExpectedTestData(const ContentCaptureData& data,
                                        int64_t expected_id) {
  ContentCaptureFrame expected(data);
  // Replaces the id with expected id.
  expected.id = expected_id;
  return expected;
}

}  // namespace content_capture
