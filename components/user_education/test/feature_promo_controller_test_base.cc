// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/test/feature_promo_controller_test_base.h"

#include <optional>

#include "base/feature_list.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/feature_promo_session_policy.h"
#include "components/user_education/common/tutorial/tutorial_service.h"
#include "components/user_education/test/mock_user_education_context.h"
#include "components/user_education/test/test_help_bubble.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"

namespace user_education::test {

BASE_FEATURE(kTestIPHFocusHelpBubbleScreenReaderPromoFeature,
             "TEST_IPH_FocusHelpBubbleScreenReaderPromo",
             base::FEATURE_ENABLED_BY_DEFAULT);

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(FeaturePromoControllerTestBase,
                                      kAnchorElementId);

// static
const ui::ElementContext FeaturePromoControllerTestBase::kAnchorElementContext =
    ui::ElementContext::CreateFakeContextForTesting(1);

// static
constexpr char
    FeaturePromoControllerTestBase::kTestFocusHelpBubbleAcceleratorPromoRead[];

FeaturePromoControllerTestBase::TestPromoControllerBase::
    TestPromoControllerBase() = default;
FeaturePromoControllerTestBase::TestPromoControllerBase::
    ~TestPromoControllerBase() = default;

FeaturePromoControllerTestBase::TestTutorialService::~TestTutorialService() =
    default;

std::u16string
FeaturePromoControllerTestBase::TestTutorialService::GetBodyIconAltText(
    bool is_last_step) const {
  return u"Body Icon Alt Text";
}

FeaturePromoControllerTestBase::FeaturePromoControllerTestBase()
    : test_promo_context_(base::MakeRefCounted<MockUserEducationContext>()) {
  storage_service_.set_clock_for_testing(task_environment_.GetMockClock());
  EXPECT_CALL(*promo_context(), IsValid).WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*promo_context(), GetElementContext)
      .WillRepeatedly(testing::Return(kAnchorElementContext));
}

FeaturePromoControllerTestBase::~FeaturePromoControllerTestBase() = default;

TestHelpBubble* FeaturePromoControllerTestBase::GetHelpBubble(
    std::optional<ui::ElementContext> context) const {
  auto* const el =
      context
          ? ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
                TestHelpBubble::kElementId, *context)
          : ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
                TestHelpBubble::kElementId);
  return el ? el->AsA<TestHelpBubbleElement>()->bubble() : nullptr;
}

void FeaturePromoControllerTestBase::SetUp() {
  using Info = FeaturePromoSessionPolicy::PromoPriorityInfo;

  session_policy_.Init(nullptr, &storage_service_);
  messaging_controller_.Init(session_provider_, storage_service_);
  promo_controller_ = CreateController();
  help_bubble_factory_registry_.MaybeRegister<TestHelpBubbleFactory>();
  anchor_element_.Show();
  EXPECT_CALL(session_policy_, CanShowPromo)
      .WillRepeatedly([](Info to_show, std::optional<Info> current) {
        return FeaturePromoSessionPolicy().CanShowPromo(to_show, current);
      });
  EXPECT_CALL(tracker_, ShouldTriggerHelpUI)
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(tracker_, WouldTriggerHelpUI)
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(tracker_, NotifyEvent).Times(testing::AnyNumber());
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(tracker_, NotifyUsedEvent).Times(testing::AnyNumber());
  EXPECT_CALL(tracker_, ListEvents)
      .WillRepeatedly(
          testing::Return(feature_engagement::Tracker::EventList()));
#endif
  EXPECT_CALL(tracker_, Dismissed).Times(testing::AnyNumber());
  const auto tracker_success = GetTrackerResult();
  EXPECT_CALL(tracker_, IsInitialized)
      .WillRepeatedly(testing::Return(tracker_success.value_or(false)));
  if (tracker_success) {
    EXPECT_CALL(tracker_, AddOnInitializedCallback)
        .WillRepeatedly(
            [tracker_success](
                feature_engagement::Tracker::OnInitializedCallback callback) {
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(
                      [](feature_engagement::Tracker::OnInitializedCallback
                             callback,
                         bool success) { std::move(callback).Run(success); },
                      std::move(callback), *tracker_success));
            });
  } else {
    EXPECT_CALL(tracker_, AddOnInitializedCallback)
        .WillRepeatedly(
            [this](
                feature_engagement::Tracker::OnInitializedCallback callback) {
              on_initialized_callbacks_.push_back(std::move(callback));
            });
  }

  // This gets called as a matter of course while retrieving certain strings,
  // but the result is not used in these tests.
  EXPECT_CALL(*promo_context(), GetAcceleratorProvider)
      .WillRepeatedly(testing::Return(nullptr));
}

std::optional<bool> FeaturePromoControllerTestBase::GetTrackerResult() const {
  return true;
}

void FeaturePromoControllerTestBase::SendTrackerResult(bool result) {
  EXPECT_CALL(tracker_, IsInitialized).WillRepeatedly(testing::Return(result));
  for (auto& callback : on_initialized_callbacks_) {
    std::move(callback).Run(result);
  }
  on_initialized_callbacks_.clear();
}

void FeaturePromoControllerTestBase::TearDown() {
  promo_controller_.reset();
}

}  // namespace user_education::test
