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
#include "ui/gfx/image/image_unittest_util.h"

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

  MOCK_METHOD(void,
              CreateToast,
              (ToastParams, const ToastSpecification*),
              (override));
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

TEST_F(ToastControllerUnitTest, ShowToast) {
  ToastRegistry* const registry = toast_registry();
  registry->RegisterToast(
      ToastId::kLinkCopied,
      ToastSpecification::Builder(vector_icons::kEmailIcon, 0).Build());

  auto controller = std::make_unique<TestToastController>(registry);

  // We should be able to show the toast because there is no toast showing.
  EXPECT_FALSE(controller->IsShowingToast());
  EXPECT_TRUE(controller->CanShowToast(ToastId::kLinkCopied));

  EXPECT_CALL(*controller, CreateToast);
  EXPECT_TRUE(controller->MaybeShowToast(ToastParams(ToastId::kLinkCopied)));
  ::testing::Mock::VerifyAndClear(controller.get());
  EXPECT_TRUE(controller->IsShowingToast());
  EXPECT_TRUE(controller->CanShowToast(ToastId::kLinkCopied));
}

TEST_F(ToastControllerUnitTest, ShowToastWithImage) {
  ToastRegistry* const registry = toast_registry();
  registry->RegisterToast(
      ToastId::kLinkCopied,
      ToastSpecification::Builder(vector_icons::kEmailIcon, 0).Build());

  auto controller = std::make_unique<TestToastController>(registry);

  // We should be able to show the toast because there is no toast showing.
  EXPECT_FALSE(controller->IsShowingToast());
  EXPECT_TRUE(controller->CanShowToast(ToastId::kLinkCopied));

  EXPECT_CALL(*controller, CreateToast);

  ToastParams params = ToastParams(ToastId::kLinkCopied);
  params.image_override =
      ui::ImageModel::FromImage(gfx::test::CreateImage(16, 16, 0xff0000));

  EXPECT_TRUE(controller->MaybeShowToast(std::move(params)));
  ::testing::Mock::VerifyAndClear(controller.get());
  EXPECT_TRUE(controller->IsShowingToast());
  EXPECT_TRUE(controller->CanShowToast(ToastId::kLinkCopied));
}

TEST_F(ToastControllerUnitTest, ToastAutomaticallyCloses) {
  ToastRegistry* const registry = toast_registry();
  registry->RegisterToast(
      ToastId::kLinkCopied,
      ToastSpecification::Builder(vector_icons::kEmailIcon, 0).Build());
  auto controller = std::make_unique<TestToastController>(registry);

  EXPECT_CALL(*controller, CreateToast);
  EXPECT_TRUE(controller->MaybeShowToast(ToastParams(ToastId::kLinkCopied)));
  ::testing::Mock::VerifyAndClear(controller.get());
  EXPECT_TRUE(controller->IsShowingToast());

  // The toast should stop showing after reaching toast timeout time.
  task_environment().FastForwardBy(
      toast_features::kToastWithoutActionTimeout.Get());
  EXPECT_FALSE(controller->IsShowingToast());
}

TEST_F(ToastControllerUnitTest, ToastWithActionButtonAutomaticallyCloses) {
  ToastRegistry* const registry = toast_registry();
  registry->RegisterToast(
      ToastId::kLinkCopied,
      ToastSpecification::Builder(vector_icons::kEmailIcon, 0).Build());
  auto controller = std::make_unique<TestToastController>(registry);

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
