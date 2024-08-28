// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toasts/toast_controller.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/api/toast_registry.h"
#include "chrome/browser/ui/toasts/api/toast_specification.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"

class ToastControllerUnitTest : public testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(toast_features::kToastFramework);
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

  auto controller = std::make_unique<ToastController>(nullptr, registry);

  // We should be able to show the toast because there is no toast showing.
  EXPECT_FALSE(controller->IsShowingToast());
  EXPECT_TRUE(controller->CanShowToast(ToastId::kLinkCopied));

  // We can show the toast again because it is an ephemeral toast.
  EXPECT_TRUE(controller->MaybeShowToast(ToastParams(ToastId::kLinkCopied)));
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

  auto controller = std::make_unique<ToastController>(nullptr, registry);

  // We should be able to show the toast because there is no toast showing.
  EXPECT_TRUE(controller->CanShowToast(ToastId::kLinkCopied));
  EXPECT_TRUE(controller->MaybeShowToast(ToastParams(ToastId::kLinkCopied)));
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

  auto controller = std::make_unique<ToastController>(nullptr, registry);
  EXPECT_TRUE(controller->MaybeShowToast(ToastParams(ToastId::kImageCopied)));
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

  auto controller = std::make_unique<ToastController>(nullptr, registry);

  // We can show the toast again because it is an ephemeral toast.
  EXPECT_TRUE(controller->MaybeShowToast(ToastParams(ToastId::kLinkCopied)));
  EXPECT_TRUE(controller->IsShowingToast());

  // The toast should stop showing after reaching toast timeout time.
  task_environment().FastForwardBy(toast_features::kToastTimeout.Get());
  EXPECT_FALSE(controller->IsShowingToast());
}

TEST_F(ToastControllerUnitTest, PersistentToastStaysOpen) {
  ToastRegistry* const registry = toast_registry();
  registry->RegisterToast(ToastId::kLinkCopied, ToastSpecification::Builder(
                                                    vector_icons::kEmailIcon, 0)
                                                    .AddPersistance()
                                                    .Build());

  auto controller = std::make_unique<ToastController>(nullptr, registry);

  EXPECT_TRUE(controller->MaybeShowToast(ToastParams(ToastId::kLinkCopied)));
  EXPECT_TRUE(controller->IsShowingToast());

  // The toast should remain showing even after past the toast timeout time.
  task_environment().FastForwardBy(toast_features::kToastTimeout.Get());
  EXPECT_TRUE(controller->IsShowingToast());

  // Persistent toasts should close when explicitly called to close.
  controller->ClosePersistentToast(ToastId::kLinkCopied);
  EXPECT_FALSE(controller->IsShowingToast());
}
