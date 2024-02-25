// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/caption_bubble_context_remote.h"

#include <optional>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace captions {
namespace {

using media::mojom::SpeechRecognitionSurface;
using testing::_;
using testing::Test;

// A surface whose methods can have expectations placed on them.
class MockSurface : public SpeechRecognitionSurface {
 public:
  explicit MockSurface(mojo::PendingReceiver<SpeechRecognitionSurface> receiver)
      : receiver_(this, std::move(receiver)) {}
  ~MockSurface() override = default;

  MockSurface(const MockSurface&) = delete;
  MockSurface& operator=(const MockSurface&) = delete;

  // media::mojom::SpeechRecognitionSurface:
  MOCK_METHOD(void, Activate, (), (override));
  MOCK_METHOD(void,
              GetBounds,
              (SpeechRecognitionSurface::GetBoundsCallback),
              (override));

 private:
  mojo::Receiver<SpeechRecognitionSurface> receiver_;
};

class CaptionBubbleContextRemoteTest : public Test {
 public:
  CaptionBubbleContextRemoteTest() = default;
  ~CaptionBubbleContextRemoteTest() override = default;

  CaptionBubbleContextRemoteTest(const CaptionBubbleContextRemoteTest&) =
      delete;
  CaptionBubbleContextRemoteTest& operator=(
      const CaptionBubbleContextRemoteTest&) = delete;

  void SetUp() override {
    mojo::PendingReceiver<SpeechRecognitionSurface> receiver;
    context_.emplace(receiver.InitWithNewPipeAndPassRemote(), "session-id");
    surface_.emplace(std::move(receiver));
  }

 protected:
  // Use optionals to delay initialization while keeping objects on the stack.
  std::optional<CaptionBubbleContextRemote> context_;
  std::optional<MockSurface> surface_;

 private:
  base::test::TaskEnvironment task_environment_;
};

// Test that activate requests are forwarded to the remote process.
TEST_F(CaptionBubbleContextRemoteTest, Activate) {
  EXPECT_CALL(*surface_, Activate()).Times(2);

  // Our expectation that the activate call is forwarded over Mojo should be
  // met.
  context_->Activate();
  context_->Activate();
  base::RunLoop().RunUntilIdle();
}

// Test that bounds requests are forwarded to the remote process.
TEST_F(CaptionBubbleContextRemoteTest, GetBounds) {
  // Note expectations are saturated from last to first.
  const gfx::Rect expected_bounds_1 = gfx::Rect(1, 2, 3, 4);
  const gfx::Rect expected_bounds_2 = gfx::Rect(5, 6, 7, 8);
  EXPECT_CALL(*surface_, GetBounds(_))
      .WillOnce([&](auto cb) { std::move(cb).Run(expected_bounds_2); })
      .RetiresOnSaturation();
  EXPECT_CALL(*surface_, GetBounds(_))
      .WillOnce([&](auto cb) { std::move(cb).Run(expected_bounds_1); })
      .RetiresOnSaturation();

  // First call should correctly fetch first bounds.
  gfx::Rect actual_bounds;
  context_->GetBounds(base::BindLambdaForTesting(
      [&](const gfx::Rect& bounds) { actual_bounds = bounds; }));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(expected_bounds_1, actual_bounds);

  // Next call should correctly fetch updated bounds.
  context_->GetBounds(base::BindLambdaForTesting(
      [&](const gfx::Rect& bounds) { actual_bounds = bounds; }));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(expected_bounds_2, actual_bounds);
}

// Test that replacing an observer is handled gracefully.
TEST_F(CaptionBubbleContextRemoteTest, DuplicateObservers) {
  bool ended_1 = false;
  bool ended_2 = false;

  // This observer will be replaced before it can execute its callback.
  auto observer_1 = context_->GetCaptionBubbleSessionObserver();
  observer_1->SetEndSessionCallback(base::BindLambdaForTesting(
      [&](const std::string& id) { ended_1 = id == "session-id"; }));

  // Creating a new observer will invalidate the old one.
  auto observer_2 = context_->GetCaptionBubbleSessionObserver();
  observer_2->SetEndSessionCallback(base::BindLambdaForTesting(
      [&](const std::string& id) { ended_2 = id == "session-id"; }));

  // Trigger session end and make sure one callback is called.
  context_->OnSessionEnded();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(ended_1);
  EXPECT_TRUE(ended_2);
}

// If the observer is dead by the time the session is ended, it shouldn't be
// exercised.
TEST_F(CaptionBubbleContextRemoteTest, DeadObserver) {
  auto observer = context_->GetCaptionBubbleSessionObserver();
  observer->SetEndSessionCallback(base::BindLambdaForTesting(
      [&](const std::string& id) { EXPECT_TRUE(false); }));
  observer.reset();

  // Trigger session end.
  context_->OnSessionEnded();
  base::RunLoop().RunUntilIdle();

  // We shouldn't try to call methods on the destructed observer.
}

}  // namespace
}  // namespace captions
