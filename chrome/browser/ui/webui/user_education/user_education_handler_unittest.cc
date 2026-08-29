// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/user_education/user_education_handler.h"

#include <memory>
#include <string_view>

#include "base/feature.h"
#include "base/functional/callback_forward.h"
#include "base/notreached.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/test/user_education/mock_browser_user_education_interface.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_registry.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/common/new_badge/new_badge_controller.h"
#include "components/user_education/common/new_badge/new_badge_specification.h"
#include "components/user_education/common/user_education_metadata.h"
#include "components/user_education/test/mock_feature_promo_controller.h"
#include "components/user_education/test/mock_new_badge_controller.h"
#include "components/user_education/webui/user_education.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace {

BASE_FEATURE(kUserEducationMixedTrustHandlerTestNewBadgeFeature1,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kUserEducationMixedTrustHandlerTestNewBadgeFeature2,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kUserEducationMixedTrustHandlerTestPromoFeature1,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kUserEducationMixedTrustHandlerTestPromoFeature2,
             base::FEATURE_ENABLED_BY_DEFAULT);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(
    kUserEducationMixedTrustHandlerTestElementId);
constexpr std::string_view kAdditionalCondition = "additional-condition";

// Handler that mocks the remote connection to the web side of the component.
// The mock is a strict mock and can be retrieved by calling the `mock()`
// method.
class TestUserEducationMixedTrustHandler
    : public UserEducationMixedTrustHandlerBase {
 public:
  TestUserEducationMixedTrustHandler() = default;
  ~TestUserEducationMixedTrustHandler() override = default;

  void set_new_badge_registry(
      user_education::NewBadgeRegistry* new_badge_registry) {
    new_badge_registry_ = new_badge_registry;
  }

  void set_new_badge_controller(
      user_education::NewBadgeController* new_badge_controller) {
    new_badge_controller_ = new_badge_controller;
  }

  void set_feature_promo_registry(
      user_education::FeaturePromoRegistry* feature_promo_registry) {
    feature_promo_registry_ = feature_promo_registry;
  }

  void set_feature_promo_controller(
      user_education::FeaturePromoController* feature_promo_controller) {
    feature_promo_controller_ = feature_promo_controller;
  }

  void set_tracker(feature_engagement::Tracker* tracker) { tracker_ = tracker; }

  void set_user_education_interface(
      BrowserUserEducationInterface* user_education_interface) {
    user_education_interface_ = user_education_interface;
  }

  const user_education::NewBadgeRegistry* GetNewBadgeRegistry() const override {
    return new_badge_registry_;
  }
  user_education::NewBadgeController* GetNewBadgeController() override {
    return new_badge_controller_;
  }
  const user_education::FeaturePromoRegistry* GetFeaturePromoRegistry()
      const override {
    return feature_promo_registry_;
  }
  user_education::FeaturePromoController* GetFeaturePromoController() override {
    return feature_promo_controller_;
  }
  feature_engagement::Tracker* GetFeatureEngagementTracker() override {
    return tracker_;
  }
  BrowserUserEducationInterface* GetBrowserUserEducationInterface() override {
    return user_education_interface_;
  }

  void ReportBadMessage(std::string_view error) override {
    ASSERT_NE("", error);
  }

 private:
  raw_ptr<user_education::NewBadgeRegistry> new_badge_registry_;
  raw_ptr<user_education::NewBadgeController> new_badge_controller_;
  raw_ptr<user_education::FeaturePromoRegistry> feature_promo_registry_;
  raw_ptr<user_education::FeaturePromoController> feature_promo_controller_;
  raw_ptr<feature_engagement::Tracker> tracker_;
  raw_ptr<BrowserUserEducationInterface> user_education_interface_;
};

}  // namespace

class UserEducationMixedTrustHandlerTest : public testing::Test {
 public:
  UserEducationMixedTrustHandlerTest() {
    const auto metadata = [] {
      return user_education::Metadata(150, "dfried@chromium.org",
                                      "Metadata for testing");
    };

    // Register some placeholder features.
    new_badge_registry_.RegisterFeature(user_education::NewBadgeSpecification(
        kUserEducationMixedTrustHandlerTestNewBadgeFeature1, metadata()));
    new_badge_registry_.RegisterFeature(user_education::NewBadgeSpecification(
        kUserEducationMixedTrustHandlerTestNewBadgeFeature2, metadata()));
    feature_promo_registry_.RegisterFeature(
        user_education::FeaturePromoSpecification::CreateForSnoozePromo(
            kUserEducationMixedTrustHandlerTestPromoFeature1,
            kUserEducationMixedTrustHandlerTestElementId, -1));
    feature_promo_registry_.RegisterFeature(
        user_education::FeaturePromoSpecification::CreateForSnoozePromo(
            kUserEducationMixedTrustHandlerTestPromoFeature2,
            kUserEducationMixedTrustHandlerTestElementId, -1));
  }

  ~UserEducationMixedTrustHandlerTest() override = default;

  void SetUp() override {
    data_host_ = std::make_unique<ui::UnownedUserDataHost>();
    browser_window_ = std::make_unique<MockBrowserWindowInterface>();
    EXPECT_CALL(*browser_window_, GetUnownedUserDataHost)
        .WillRepeatedly(testing::ReturnRef(*data_host_));
    user_education_interface_ = std::make_unique<
        testing::StrictMock<MockBrowserUserEducationInterface>>(
        browser_window_.get());
    handler_ = std::make_unique<TestUserEducationMixedTrustHandler>();
    handler_->set_new_badge_registry(&new_badge_registry_);
    handler_->set_new_badge_controller(&new_badge_controller_);
    handler_->set_feature_promo_registry(&feature_promo_registry_);
    handler_->set_feature_promo_controller(&feature_promo_controller_);
    handler_->set_tracker(&tracker_);
    handler_->set_user_education_interface(user_education_interface_.get());
  }

  void TearDown() override { handler_.reset(); }

  void RemoveControllers() {
    handler_->set_new_badge_registry(nullptr);
    handler_->set_new_badge_controller(nullptr);
    handler_->set_feature_promo_registry(nullptr);
    handler_->set_feature_promo_controller(nullptr);
    handler_->set_tracker(nullptr);
    handler_->set_user_education_interface(nullptr);
    user_education_interface_.reset();
    browser_window_.reset();
    data_host_.reset();
  }

 protected:
  user_education::NewBadgeRegistry new_badge_registry_;
  testing::StrictMock<user_education::test::MockNewBadgeController>
      new_badge_controller_;
  user_education::FeaturePromoRegistry feature_promo_registry_;
  testing::StrictMock<user_education::test::MockFeaturePromoController>
      feature_promo_controller_;
  testing::StrictMock<feature_engagement::test::MockTracker> tracker_;
  std::unique_ptr<ui::UnownedUserDataHost> data_host_;
  std::unique_ptr<MockBrowserWindowInterface> browser_window_;
  std::unique_ptr<testing::StrictMock<MockBrowserUserEducationInterface>>
      user_education_interface_;
  std::unique_ptr<TestUserEducationMixedTrustHandler> handler_;
};

TEST_F(UserEducationMixedTrustHandlerTest, MaybeShowFeaturePromo) {
  constexpr std::string kKey = "key";
  EXPECT_CALL(
      *user_education_interface_,
      MaybeShowFeaturePromo(testing::AllOf(
          testing::ResultOf(
              [](const user_education::FeaturePromoParams& params)
                  -> const base::Feature& { return *params.feature; },
              testing::Ref(kUserEducationMixedTrustHandlerTestPromoFeature1)),
          testing::Field(&user_education::FeaturePromoParams::key, kKey))));
  handler_->MaybeShowFeaturePromo(
      user_education::mojom::FeaturePromoParams::New(
          kUserEducationMixedTrustHandlerTestPromoFeature1.name, kKey));
}

TEST_F(UserEducationMixedTrustHandlerTest,
       NotifyFeaturePromoFeatureUsed_IgnoreFeatureIfPresent) {
  EXPECT_CALL(feature_promo_controller_,
              NotifyFeatureUsedIfValid(testing::Ref(
                  kUserEducationMixedTrustHandlerTestPromoFeature1)));
  handler_->NotifyFeaturePromoFeatureUsed(
      kUserEducationMixedTrustHandlerTestPromoFeature1.name,
      user_education::mojom::FeaturePromoFeatureUsedAction::
          kIgnorePromoIfPresent);
}

TEST_F(UserEducationMixedTrustHandlerTest,
       NotifyFeaturePromoFeatureUsed_ClosePromoIfPresent) {
  EXPECT_CALL(feature_promo_controller_,
              NotifyFeatureUsedIfValid(testing::Ref(
                  kUserEducationMixedTrustHandlerTestPromoFeature2)));
  EXPECT_CALL(
      feature_promo_controller_,
      EndPromo(testing::Ref(kUserEducationMixedTrustHandlerTestPromoFeature2),
               user_education::EndFeaturePromoReason::kFeatureEngaged));
  handler_->NotifyFeaturePromoFeatureUsed(
      kUserEducationMixedTrustHandlerTestPromoFeature2.name,
      user_education::mojom::FeaturePromoFeatureUsedAction::
          kClosePromoIfPresent);
}

TEST_F(UserEducationMixedTrustHandlerTest, NotifyAdditionalConditionEvent) {
  EXPECT_CALL(tracker_, NotifyEvent(std::string(kAdditionalCondition)));
  handler_->NotifyAdditionalConditionEvent(std::string(kAdditionalCondition));
}

TEST_F(UserEducationMixedTrustHandlerTest, NotifyNewBadgeFeatureUsed) {
  EXPECT_CALL(new_badge_controller_,
              NotifyFeatureUsed(testing::Ref(
                  kUserEducationMixedTrustHandlerTestNewBadgeFeature1)));
  handler_->NotifyNewBadgeFeatureUsed(
      kUserEducationMixedTrustHandlerTestNewBadgeFeature1.name);
}

TEST_F(UserEducationMixedTrustHandlerTest, MaybeShowNewBadgeFor_ReturnsTrue) {
  using CallbackType = base::OnceCallback<void(bool)>;
  UNCALLED_MOCK_CALLBACK(CallbackType, callback);
  EXPECT_CALL(new_badge_controller_,
              MaybeShowNewBadge(testing::Ref(
                  kUserEducationMixedTrustHandlerTestNewBadgeFeature2)))
      .WillOnce(testing::Return(
          user_education::DisplayNewBadge::create_for_test(true)));
  EXPECT_CALL_IN_SCOPE(
      callback, Run(true),
      handler_->MaybeShowNewBadgeFor(
          kUserEducationMixedTrustHandlerTestNewBadgeFeature2.name,
          callback.Get()));
}

TEST_F(UserEducationMixedTrustHandlerTest, MaybeShowNewBadgeFor_ReturnsFalse) {
  using CallbackType = base::OnceCallback<void(bool)>;
  UNCALLED_MOCK_CALLBACK(CallbackType, callback);
  EXPECT_CALL(new_badge_controller_,
              MaybeShowNewBadge(testing::Ref(
                  kUserEducationMixedTrustHandlerTestNewBadgeFeature2)))
      .WillOnce(testing::Return(
          user_education::DisplayNewBadge::create_for_test(false)));
  EXPECT_CALL_IN_SCOPE(
      callback, Run(false),
      handler_->MaybeShowNewBadgeFor(
          kUserEducationMixedTrustHandlerTestNewBadgeFeature2.name,
          callback.Get()));
}

TEST_F(UserEducationMixedTrustHandlerTest, NoControllers) {
  using CallbackType = base::OnceCallback<void(bool)>;
  UNCALLED_MOCK_CALLBACK(CallbackType, callback);
  RemoveControllers();
  handler_->MaybeShowFeaturePromo(
      user_education::mojom::FeaturePromoParams::New(
          kUserEducationMixedTrustHandlerTestPromoFeature1.name, std::nullopt));
  handler_->NotifyFeaturePromoFeatureUsed(
      kUserEducationMixedTrustHandlerTestPromoFeature1.name,
      user_education::mojom::FeaturePromoFeatureUsedAction::
          kClosePromoIfPresent);
  handler_->NotifyNewBadgeFeatureUsed(
      kUserEducationMixedTrustHandlerTestNewBadgeFeature1.name);
  EXPECT_CALL_IN_SCOPE(
      callback, Run(false),
      handler_->MaybeShowNewBadgeFor(
          kUserEducationMixedTrustHandlerTestNewBadgeFeature2.name,
          callback.Get()));
}
