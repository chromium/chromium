// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toasts/toast_controller.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/api/toast_registry.h"
#include "chrome/browser/ui/toasts/api/toast_specification.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class TestToastController : public ToastController {
 public:
  explicit TestToastController(ToastRegistry* toast_registry)
      : ToastController(nullptr, toast_registry) {}

  void CloseToast(toasts::ToastCloseReason reason) override {
    if (IsShowingToast()) {
      OnWidgetDestroyed(nullptr);
    }
  }

  MOCK_METHOD2(CreateToast,
               void(const ToastParams& params, const ToastSpecification* spec));
};
}  // namespace

class ToastControllerUnitTest : public testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        toast_features::kToastFramework,
        {{toast_features::kToastWithoutActionTimeout.name, "8s"}});
    toast_registry_ = std::make_unique<ToastRegistry>();
  }

  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

  ToastRegistry* toast_registry() { return toast_registry_.get(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<ToastRegistry> toast_registry_;
};

TEST_F(ToastControllerUnitTest, ShowEphemeralToast) {
  ToastRegistry* const registry = toast_registry();
  registry->RegisterToast(
      ToastId::kLinkCopied,
      ToastSpecification::Builder(vector_icons::kEmailIcon, 0).Build());

  auto controller = std::make_unique<TestToastController>(registry);

  // We should be able to show the toast because there is no toast showing.
  EXPECT_FALSE(controller->IsShowingToast());
  EXPECT_TRUE(controller->CanShowToast(ToastId::kLinkCopied));

  // We can show the toast again because it is an ephemeral toast.
  EXPECT_CALL(*controller, CreateToast);
  EXPECT_TRUE(controller->MaybeShowToast(ToastParams(ToastId::kLinkCopied)));
  ::testing::Mock::VerifyAndClear(controller.get());
  EXPECT_TRUE(controller->IsShowingToast());
  EXPECT_TRUE(controller->CanShowToast(ToastId::kLinkCopied));
}

TEST_F(ToastControllerUnitTest, ShowPersistentToast) {
  ToastRegistry* const registry = toast_registry();
  registry->RegisterToast(ToastId::kLinkCopied, ToastSpecification::Builder(
                                                    vector_icons::kEmailIcon, 0)
                                                    .AddPersistance()
                                                    .Build());

  registry->RegisterToast(
      ToastId::kImageCopied,
      ToastSpecification::Builder(vector_icons::kEmailIcon, 0)
          .AddPersistance()
          .Build());

  auto controller = std::make_unique<TestToastController>(registry);

  // We should be able to show the toast because there is no toast showing.
  EXPECT_TRUE(controller->CanShowToast(ToastId::kLinkCopied));
  EXPECT_CALL(*controller, CreateToast);
  EXPECT_TRUE(controller->MaybeShowToast(ToastParams(ToastId::kLinkCopied)));
  ::testing::Mock::VerifyAndClear(controller.get());
  EXPECT_TRUE(controller->IsShowingToast());

  // We should not be able to trigger the same same toast to show or another
  // persistent toast because we are already showing a persistent toast.
  EXPECT_FALSE(controller->CanShowToast(ToastId::kLinkCopied));
  EXPECT_FALSE(controller->CanShowToast(ToastId::kImageCopied));
}

TEST_F(ToastControllerUnitTest, PreemptPersistentToast) {
  ToastRegistry* const registry = toast_registry();
  registry->RegisterToast(
      ToastId::kLinkCopied,
      ToastSpecification::Builder(vector_icons::kEmailIcon, 0).Build());
  registry->RegisterToast(
      ToastId::kImageCopied,
      ToastSpecification::Builder(vector_icons::kEmailIcon, 0)
          .AddPersistance()
          .Build());

  auto controller = std::make_unique<TestToastController>(registry);
  EXPECT_CALL(*controller, CreateToast);
  EXPECT_TRUE(controller->MaybeShowToast(ToastParams(ToastId::kImageCopied)));
  ::testing::Mock::VerifyAndClear(controller.get());
  EXPECT_TRUE(controller->IsShowingToast());

  // The ephemeral toast can show but the persistent toast cannot show while we
  // are currently showing a persistent toast.
  EXPECT_TRUE(controller->CanShowToast(ToastId::kLinkCopied));
  EXPECT_FALSE(controller->CanShowToast(ToastId::kImageCopied));
}

TEST_F(ToastControllerUnitTest, EphemeralToastAutomaticallyCloses) {
  ToastRegistry* const registry = toast_registry();
  registry->RegisterToast(
      ToastId::kLinkCopied,
      ToastSpecification::Builder(vector_icons::kEmailIcon, 0).Build());
  auto controller = std::make_unique<TestToastController>(registry);

  // We can show the toast again because it is an ephemeral toast.
  EXPECT_CALL(*controller, CreateToast);
  EXPECT_TRUE(controller->MaybeShowToast(ToastParams(ToastId::kLinkCopied)));
  ::testing::Mock::VerifyAndClear(controller.get());
  EXPECT_TRUE(controller->IsShowingToast());

  // The toast should stop showing after reaching toast timeout time.
  task_environment().FastForwardBy(
      toast_features::kToastWithoutActionTimeout.Get());
  EXPECT_FALSE(controller->IsShowingToast());
}

TEST_F(ToastControllerUnitTest,
       EphemeralToastWithActionButtonAutomaticallyCloses) {
  ToastRegistry* const registry = toast_registry();
  registry->RegisterToast(
      ToastId::kLinkCopied,
      ToastSpecification::Builder(vector_icons::kEmailIcon, 0).Build());
  auto controller = std::make_unique<TestToastController>(registry);

  // We can show the toast again because it is an ephemeral toast.
  EXPECT_CALL(*controller, CreateToast);
  EXPECT_TRUE(controller->MaybeShowToast(ToastParams(ToastId::kLinkCopied)));
  ::testing::Mock::VerifyAndClear(controller.get());
  EXPECT_TRUE(controller->IsShowingToast());

  // The toast should stop showing after reaching toast timeout time.
  task_environment().FastForwardBy(toast_features::kToastTimeout.Get());
  EXPECT_FALSE(controller->IsShowingToast());
}

TEST_F(ToastControllerUnitTest, CloseTimerResetsWhenToastShown) {
  ToastRegistry* const registry = toast_registry();
  registry->RegisterToast(
      ToastId::kLinkCopied,
      ToastSpecification::Builder(vector_icons::kEmailIcon, 0).Build());
  registry->RegisterToast(
      ToastId::kImageCopied,
      ToastSpecification::Builder(vector_icons::kEmailIcon, 0).Build());

  auto controller = std::make_unique<TestToastController>(registry);

  // We can show the toast again because it is an ephemeral toast.
  EXPECT_CALL(*controller, CreateToast);
  EXPECT_TRUE(controller->MaybeShowToast(ToastParams(ToastId::kLinkCopied)));
  ::testing::Mock::VerifyAndClear(controller.get());
  EXPECT_TRUE(controller->IsShowingToast());

  // The toast should still be showing because we didn't reach the time out time
  // yet.
  task_environment().FastForwardBy(toast_features::kToastTimeout.Get() / 2);
  EXPECT_TRUE(controller->IsShowingToast());

  // Show a different toast before the link copied toast times out.
  EXPECT_CALL(*controller, CreateToast);
  EXPECT_TRUE(controller->MaybeShowToast(ToastParams(ToastId::kImageCopied)));
  ::testing::Mock::VerifyAndClear(controller.get());
  EXPECT_TRUE(controller->IsShowingToast());

  // The image copied toast should still be showing even though the link copied
  // toast should have timed out by now.
  task_environment().FastForwardBy(toast_features::kToastTimeout.Get() / 2);
  EXPECT_TRUE(controller->IsShowingToast());
}

TEST_F(ToastControllerUnitTest, PersistentToastStaysOpen) {
  ToastRegistry* const registry = toast_registry();
  registry->RegisterToast(ToastId::kLinkCopied, ToastSpecification::Builder(
                                                    vector_icons::kEmailIcon, 0)
                                                    .AddPersistance()
                                                    .Build());

  auto controller = std::make_unique<TestToastController>(registry);

  EXPECT_CALL(*controller, CreateToast);
  EXPECT_TRUE(controller->MaybeShowToast(ToastParams(ToastId::kLinkCopied)));
  ::testing::Mock::VerifyAndClear(controller.get());
  EXPECT_TRUE(controller->IsShowingToast());

  // The toast should remain showing even after past the toast timeout time.
  task_environment().FastForwardBy(toast_features::kToastTimeout.Get());
  EXPECT_TRUE(controller->IsShowingToast());

  // Persistent toasts should close when explicitly called to close.
  controller->ClosePersistentToast(ToastId::kLinkCopied);
  EXPECT_FALSE(controller->IsShowingToast());
}

TEST_F(ToastControllerUnitTest, ClosePersistentToast) {
  ToastRegistry* const registry = toast_registry();
  registry->RegisterToast(ToastId::kLinkCopied, ToastSpecification::Builder(
                                                    vector_icons::kEmailIcon, 0)
                                                    .AddPersistance()
                                                    .Build());

  auto controller = std::make_unique<TestToastController>(registry);
  EXPECT_CALL(*controller, CreateToast);
  EXPECT_TRUE(controller->MaybeShowToast(ToastParams(ToastId::kLinkCopied)));
  ::testing::Mock::VerifyAndClear(controller.get());
  EXPECT_TRUE(controller->IsShowingToast());

  controller->ClosePersistentToast(ToastId::kLinkCopied);
  EXPECT_FALSE(controller->IsShowingToast());
  // Trying to close the persistent toast should crash since the toast is
  // already closed.
  EXPECT_DEATH(controller->ClosePersistentToast(ToastId::kLinkCopied), "");
}
