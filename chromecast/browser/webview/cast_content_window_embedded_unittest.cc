// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webview/cast_content_window_embedded.h"

#include <memory>
#include <utility>

#include "chromecast/browser/test/mock_cast_web_view.h"
#include "chromecast/browser/webview/cast_window_embedder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace {
using testing::_;
using ::testing::Return;

class MockCastWindowEmbedder : public CastWindowEmbedder {
 public:
  MockCastWindowEmbedder() = default;
  ~MockCastWindowEmbedder() override = default;

  // CastWindowEmbedder implementation:
  MOCK_METHOD(int, GenerateWindowId, (), (override));

  MOCK_METHOD(void,
              AddEmbeddedWindow,
              (CastWindowEmbedder::EmbeddedWindow*),
              (override));

  MOCK_METHOD(void,
              RemoveEmbeddedWindow,
              (CastWindowEmbedder::EmbeddedWindow*),
              (override));

  MOCK_METHOD(void,
              OnWindowRequest,
              (const CastWindowEmbedder::WindowRequestType&,
               const CastWindowEmbedder::CastWindowProperties&),
              (override));

  MOCK_METHOD(void,
              GenerateAndSendNavigationHandleResult,
              (const int,
               const std::string,
               const bool,
               CastWindowEmbedder::NavigationType),
              (override));
};

mojom::CastWebViewParamsPtr GenerateParams() {
  mojom::CastWebViewParamsPtr params = mojom::CastWebViewParams::New();
  params->enable_touch_input = true;
  params->is_remote_control_mode = false;
  params->turn_on_screen = true;
  params->gesture_priority = mojom::GesturePriority::MAIN_ACTIVITY;
  params->session_id = "test_session_id";
  return params;
}

}  // namespace

class CastContentWindowEmbeddedTest : public testing::Test {
 public:
  CastContentWindowEmbeddedTest() {}
  ~CastContentWindowEmbeddedTest() override = default;

  void SetUp() override {
    cast_window_embedder_ = std::make_unique<MockCastWindowEmbedder>();
  }

  std::unique_ptr<MockCastWindowEmbedder> cast_window_embedder_;
  std::unique_ptr<CastContentWindowEmbedded> cast_content_window_embedded_;
};

// Test 1: Embedded window shall add itself into the CastWindowEmbedder's
// list of managed windows.
TEST_F(CastContentWindowEmbeddedTest, AddObserverOnWindowCreation) {
  EXPECT_CALL(*cast_window_embedder_, AddEmbeddedWindow(_)).Times(1);

  cast_content_window_embedded_ = std::make_unique<CastContentWindowEmbedded>(
      GenerateParams(), cast_window_embedder_.get(),
      false /* force_720p_resolution */);
}

// Test 2: Embedded window shall remove itself from the CastWindowEmbedder's
// list of managed windows.
TEST_F(CastContentWindowEmbeddedTest, RemoveObserverOnWindowClose) {
  EXPECT_CALL(*cast_window_embedder_, RemoveEmbeddedWindow(_)).Times(1);

  cast_content_window_embedded_ = std::make_unique<CastContentWindowEmbedded>(
      GenerateParams(), cast_window_embedder_.get(),
      false /* force_720p_resolution */);
  cast_content_window_embedded_.reset();
}

// Test 3: Embedded window shall request the embedder to assign a unique window
// ID.
TEST_F(CastContentWindowEmbeddedTest,
       RequestToGenerateWindowIdOnWindowCreation) {
  EXPECT_CALL(*cast_window_embedder_, AddEmbeddedWindow(_)).Times(1);
  EXPECT_CALL(*cast_window_embedder_, GenerateWindowId()).Times(1);

  cast_content_window_embedded_ = std::make_unique<CastContentWindowEmbedded>(
      GenerateParams(), cast_window_embedder_.get(),
      false /* force_720p_resolution */);
}

// Test 4: Delegate can handle navigation GO_BACK event. In this case,
// the embedded shall inform the embedder that the navigation request
// has been handled successfully.
TEST_F(CastContentWindowEmbeddedTest, HandleNavigationEventByDelegate) {
  // Use a fake window_id here for testing.
  constexpr int fake_window_id = 1;

  EXPECT_CALL(*cast_window_embedder_, AddEmbeddedWindow(_)).Times(1);
  EXPECT_CALL(*cast_window_embedder_, GenerateWindowId())
      .Times(1)
      .WillOnce(Return(fake_window_id /* window_id */));

  cast_content_window_embedded_ = std::make_unique<CastContentWindowEmbedded>(
      GenerateParams(), cast_window_embedder_.get(),
      false /* force_720p_resolution */);

  cast_content_window_embedded_->gesture_router()->SetCanGoBack(true);
  cast_content_window_embedded_->gesture_router()->SetConsumeGestureCallback(
      base::BindRepeating(
          [](GestureType gesture_type,
             base::OnceCallback<void(bool)> consume_gesture_completed) {
            std::move(consume_gesture_completed).Run(true);
          }));

  // CastWindowEmbedder shall receive the ack of event handling
  EXPECT_CALL(
      *cast_window_embedder_,
      GenerateAndSendNavigationHandleResult(
          fake_window_id, _, true, CastWindowEmbedder::NavigationType::GO_BACK))
      .Times(1);

  CastWindowEmbedder::EmbedderWindowEvent window_event;
  window_event.window_id = 1;
  window_event.navigation = CastWindowEmbedder::NavigationType::GO_BACK;
  cast_content_window_embedded_->OnEmbedderWindowEvent(window_event);
}

// Test 5: Delegate cannot consume navigation GO_BACK event. In this case,
// the embedded window shall inform the embedder that the navigation request
// has been rejected / not handled.
TEST_F(CastContentWindowEmbeddedTest, NavigationEventUnhandled) {
  // Use a fake window_id here for testing.
  constexpr int fake_window_id = 1;

  EXPECT_CALL(*cast_window_embedder_, AddEmbeddedWindow(_)).Times(1);
  EXPECT_CALL(*cast_window_embedder_, GenerateWindowId())
      .Times(1)
      .WillOnce(Return(fake_window_id /* window_id */));

  cast_content_window_embedded_ = std::make_unique<CastContentWindowEmbedded>(
      GenerateParams(), cast_window_embedder_.get(),
      false /* force_720p_resolution */);

  cast_content_window_embedded_->gesture_router()->SetCanGoBack(true);

  // Delegate of the CastContentWindow indicts that it dit not handle the
  // event.
  cast_content_window_embedded_->gesture_router()->SetConsumeGestureCallback(
      base::BindRepeating(
          [](GestureType gesture_type,
             base::OnceCallback<void(bool)> consume_gesture_completed) {
            std::move(consume_gesture_completed).Run(false);
          }));

  // CastWindowEmbedder shall receive the ack of event handling
  EXPECT_CALL(*cast_window_embedder_,
              GenerateAndSendNavigationHandleResult(
                  fake_window_id, _, false /* handled */,
                  CastWindowEmbedder::NavigationType::GO_BACK))
      .Times(1);

  CastWindowEmbedder::EmbedderWindowEvent window_event;
  window_event.window_id = 1;
  window_event.navigation = CastWindowEmbedder::NavigationType::GO_BACK;
  cast_content_window_embedded_->OnEmbedderWindowEvent(window_event);
}

// Test 6: Delegate cannot handle GO_BACK gesture. In this case,
// the embedded window shall inform the embedder that the navigation request
// has been rejected / not handled.
TEST_F(CastContentWindowEmbeddedTest, DelegateCannotHandleGoBackGesture) {
  // Use a fake window_id here for testing.
  constexpr int fake_window_id = 1;

  EXPECT_CALL(*cast_window_embedder_, AddEmbeddedWindow(_)).Times(1);
  EXPECT_CALL(*cast_window_embedder_, GenerateWindowId())
      .Times(1)
      .WillOnce(Return(fake_window_id /* window_id */));

  cast_content_window_embedded_ = std::make_unique<CastContentWindowEmbedded>(
      GenerateParams(), cast_window_embedder_.get(),
      false /* force_720p_resolution */);

  cast_content_window_embedded_->gesture_router()->SetCanGoBack(false);

  // CastWindowEmbedder shall receive the ack of event handling
  EXPECT_CALL(*cast_window_embedder_,
              GenerateAndSendNavigationHandleResult(
                  fake_window_id, _, false /* handled */,
                  CastWindowEmbedder::NavigationType::GO_BACK))
      .Times(1);

  CastWindowEmbedder::EmbedderWindowEvent window_event;
  window_event.window_id = 1;
  window_event.navigation = CastWindowEmbedder::NavigationType::GO_BACK;
  cast_content_window_embedded_->OnEmbedderWindowEvent(window_event);
}

// Test 7: |GetWindowId()| must be same as the value which is assigned by
// |CastWindowEmbedder::GenerateId()|.
TEST_F(CastContentWindowEmbeddedTest, WindowIdMustMatchInitialValue) {
  // Use a fake window_id here for testing.
  constexpr int fake_window_id = 369;

  EXPECT_CALL(*cast_window_embedder_, AddEmbeddedWindow(_)).Times(1);
  EXPECT_CALL(*cast_window_embedder_, GenerateWindowId())
      .Times(1)
      .WillOnce(Return(fake_window_id /* window_id */));

  cast_content_window_embedded_ = std::make_unique<CastContentWindowEmbedded>(
      GenerateParams(), cast_window_embedder_.get(),
      false /* force_720p_resolution */);

  EXPECT_EQ(fake_window_id, cast_content_window_embedded_->GetWindowId());
}

}  // namespace chromecast
