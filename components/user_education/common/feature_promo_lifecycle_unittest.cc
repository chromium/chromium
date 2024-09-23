// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo_lifecycle.h"

#include <sstream>
#include <tuple>
#include <type_traits>

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/user_education/common/feature_promo_data.h"
#include "components/user_education/common/feature_promo_result.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "components/user_education/common/feature_promo_storage_service.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/test/test_feature_promo_storage_service.h"
#include "components/user_education/test/test_help_bubble.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"

namespace user_education {

namespace {
BASE_FEATURE(kTestIPHFeature,
             "TestIPHFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTestIPHFeature2,
             "TestIPHFeature2",
             base::FEATURE_ENABLED_BY_DEFAULT);
constexpr char kAppName[] = "App1";
constexpr char kAppName2[] = "App2";
constexpr int kNumRotatingPromos = 3;
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestElementId);
const ui::ElementContext kTestElementContext{1};

template <typename Arg, typename... Args>
std::string ParamToString(
    const testing::TestParamInfo<std::tuple<Arg, Args...>>& param) {
  std::ostringstream oss;
  oss << std::get<Arg>(param.param);
  ((oss << "_" << std::get<Args>(param.param)), ...);
  return oss.str();
}

}  // namespace

using PromoType = FeaturePromoSpecification::PromoType;
using PromoSubtype = FeaturePromoSpecification::PromoSubtype;
using CloseReason = FeaturePromoClosedReason;

class FeaturePromoLifecycleTest : public testing::Test {
 public:
  FeaturePromoLifecycleTest() = default;
  ~FeaturePromoLifecycleTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    feature_list_.InitAndEnableFeature(
        features::kUserEducationExperienceVersion2);
    element_.Show();
  }

  void TearDown() override {
    ASSERT_EQ(0, num_open_bubbles_);
    testing::Test::TearDown();
  }

  PromoType promo_type() const { return promo_type_; }
  void set_promo_type(PromoType promo_type) { promo_type_ = promo_type; }
  PromoSubtype promo_subtype() const { return promo_subtype_; }
  void set_promo_subtype(PromoSubtype promo_subtype) {
    promo_subtype_ = promo_subtype;
  }
  void set_reshow_params(base::TimeDelta reshow_delay,
                         std::optional<int> max_show_count) {
    reshow_delay_ = reshow_delay;
    max_show_count_ = max_show_count;
  }

  FeaturePromoResult GetSnoozedResult() const {
    return promo_subtype() == PromoSubtype::kNormal &&
                   (promo_type() == PromoType::kSnooze ||
                    promo_type() == PromoType::kTutorial ||
                    promo_type() == PromoType::kCustomAction)
               ? FeaturePromoResult::kSnoozed
               : FeaturePromoResult::Success();
  }

  FeaturePromoResult GetNonInteractedResult() const {
    return promo_subtype() == PromoSubtype::kNormal &&
                   (promo_type() == PromoType::kSnooze ||
                    promo_type() == PromoType::kTutorial ||
                    promo_type() == PromoType::kCustomAction)
               ? FeaturePromoResult::kRecentlyAborted
               : FeaturePromoResult::Success();
  }

  FeaturePromoResult GetExceededResult() const {
    return promo_subtype() == PromoSubtype::kNormal
               ? FeaturePromoResult::kExceededMaxShowCount
               : FeaturePromoResult::Success();
  }

  FeaturePromoResult GetNewProfileResult() const {
    return promo_subtype() == PromoSubtype::kNormal
               ? FeaturePromoResult::kBlockedByNewProfile
               : FeaturePromoResult::Success();
  }

  std::unique_ptr<FeaturePromoLifecycle> CreateLifecycle(
      const base::Feature& feature,
      const char* app_id = nullptr) {
    if (!app_id) {
      app_id = promo_subtype() == PromoSubtype::kKeyedNotice ? kAppName : "";
    }
    return MaybeSetReshowParams(std::make_unique<FeaturePromoLifecycle>(
        &storage_service_, app_id, &feature, promo_type(), promo_subtype(),
        promo_type() == PromoType::kRotating ? kNumRotatingPromos : 0));
  }

  std::unique_ptr<test::TestHelpBubble> CreateHelpBubble() {
    ++num_open_bubbles_;
    auto result =
        std::make_unique<test::TestHelpBubble>(&element_, HelpBubbleParams());
    help_bubble_subscriptions_.emplace_back(
        result->AddOnCloseCallback(base::BindLambdaForTesting(
            [this](HelpBubble*, HelpBubble::CloseReason) {
              --num_open_bubbles_;
            })));
    return result;
  }

  auto CheckShownMetrics(const FeaturePromoLifecycle* lifecycle,
                         int shown_count,
                         std::map<int, int> rotating_counts = {}) {
    EXPECT_EQ(shown_count,
              user_action_tester_.GetActionCount("UserEducation.MessageShown"));
    EXPECT_EQ(
        shown_count,
        user_action_tester_.GetActionCount(base::StringPrintf(
            "UserEducation.MessageShown.%s", lifecycle->iph_feature()->name)));

    std::string name = "";
    switch (lifecycle->promo_subtype()) {
      case PromoSubtype::kKeyedNotice:
        name = "KeyedNotice.";
        break;
      case PromoSubtype::kLegalNotice:
        name = "LegalNotice.";
        break;
      case PromoSubtype::kActionableAlert:
        name = "ActionableAlert.";
        break;
      case PromoSubtype::kNormal:
        break;
    }

    switch (lifecycle->promo_type()) {
      case PromoType::kLegacy:
        name.append("Legacy");
        break;
      case PromoType::kToast:
        name.append("Toast");
        break;
      case PromoType::kCustomAction:
        name.append("CustomAction");
        break;
      case PromoType::kSnooze:
        name.append("Snooze");
        break;
      case PromoType::kTutorial:
        name.append("Tutorial");
        break;
      case PromoType::kRotating:
        name.append("Rotating");
        break;
      case PromoType::kUnspecified:
        NOTREACHED_IN_MIGRATION();
        return;
    }

    EXPECT_EQ(shown_count,
              user_action_tester_.GetActionCount(base::StringPrintf(
                  "UserEducation.MessageShown.%s", name.data())));
    histogram_tester_.ExpectBucketCount(
        "UserEducation.MessageShown.Type",
        static_cast<int>(lifecycle->promo_type()), shown_count);
    histogram_tester_.ExpectBucketCount(
        "UserEducation.MessageShown.Subtype",
        static_cast<int>(lifecycle->promo_subtype()), shown_count);

    const std::string rotating_histogram_name = base::StringPrintf(
        "UserEducation.RotatingPromoIndex.%s", lifecycle->iph_feature()->name);
    int total_count = 0;
    for (const auto& [index, count] : rotating_counts) {
      histogram_tester_.ExpectBucketCount(rotating_histogram_name, index + 1,
                                          count);
      total_count += count;
    }
    histogram_tester_.ExpectTotalCount(rotating_histogram_name, total_count);
  }

 protected:
  base::SimpleTestClock& UseTestClock() {
    storage_service_.set_clock_for_testing(&clock_);
    return clock_;
  }

  std::unique_ptr<FeaturePromoLifecycle> MaybeSetReshowParams(
      std::unique_ptr<FeaturePromoLifecycle> lifecycle) {
    if (reshow_delay_) {
      lifecycle->SetReshowPolicy(*reshow_delay_, max_show_count_);
    }
    return lifecycle;
  }

  PromoType promo_type_ = PromoType::kSnooze;
  PromoSubtype promo_subtype_ = PromoSubtype::kNormal;
  std::optional<base::TimeDelta> reshow_delay_;
  std::optional<int> max_show_count_;
  int num_open_bubbles_ = 0;
  ui::test::TestElement element_{kTestElementId, kTestElementContext};
  test::TestFeaturePromoStorageService storage_service_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  testing::StrictMock<feature_engagement::test::MockTracker> tracker_;
  std::vector<base::CallbackListSubscription> help_bubble_subscriptions_;
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;

  base::test::ScopedFeatureList feature_list_;

 private:
  base::SimpleTestClock clock_;
};

TEST_F(FeaturePromoLifecycleTest, BubbleClosedOnDiscard) {
  auto lifecycle = CreateLifecycle(kTestIPHFeature);
  lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);
  EXPECT_CALL(tracker_, Dismissed(testing::Ref(kTestIPHFeature)));
  CheckShownMetrics(lifecycle.get(), /*shown_count=*/1);
  lifecycle.reset();
  EXPECT_EQ(0, num_open_bubbles_);
}

TEST_F(FeaturePromoLifecycleTest, BubbleClosedOnContinue) {
  auto lifecycle = CreateLifecycle(kTestIPHFeature);
  lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);
  lifecycle->OnPromoEnded(CloseReason::kFeatureEngaged, true);
  EXPECT_EQ(0, num_open_bubbles_);
  auto promo_data = storage_service_.ReadPromoData(kTestIPHFeature);
  EXPECT_TRUE(promo_data->is_dismissed);
  EXPECT_CALL(tracker_, Dismissed(testing::Ref(kTestIPHFeature)));
}

TEST_F(FeaturePromoLifecycleTest,
       ClosePromoBubbleAndContinue_kNormal_TutorialSucceeds) {
  set_promo_type(PromoType::kTutorial);
  auto lifecycle = CreateLifecycle(kTestIPHFeature);
  lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);
  lifecycle->OnPromoEnded(CloseReason::kAction, true);
  auto promo_data = storage_service_.ReadPromoData(kTestIPHFeature);
  EXPECT_FALSE(promo_data->is_dismissed);

  EXPECT_CALL(tracker_, Dismissed(testing::Ref(kTestIPHFeature)));
  lifecycle->OnContinuedPromoEnded(true);
  CheckShownMetrics(lifecycle.get(), /*shown_count=*/1);
  promo_data = storage_service_.ReadPromoData(kTestIPHFeature);
  EXPECT_TRUE(promo_data->is_dismissed);
  EXPECT_EQ(CloseReason::kAction, promo_data->last_dismissed_by);
}

TEST_F(FeaturePromoLifecycleTest,
       ClosePromoBubbleAndContinue_kNormal_TutorialFails) {
  set_promo_type(PromoType::kTutorial);
  auto lifecycle = CreateLifecycle(kTestIPHFeature);
  lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);
  lifecycle->OnPromoEnded(CloseReason::kAction, true);
  auto promo_data = storage_service_.ReadPromoData(kTestIPHFeature);
  EXPECT_FALSE(promo_data->is_dismissed);

  EXPECT_CALL(tracker_, Dismissed(testing::Ref(kTestIPHFeature)));
  lifecycle->OnContinuedPromoEnded(false);
  promo_data = storage_service_.ReadPromoData(kTestIPHFeature);
  EXPECT_FALSE(promo_data->is_dismissed);
  EXPECT_EQ(1, promo_data->snooze_count);
}

TEST_F(FeaturePromoLifecycleTest,
       ClosePromoBubbleAndContinue_DismissOnDiscard) {
  set_promo_type(PromoType::kTutorial);
  auto lifecycle = CreateLifecycle(kTestIPHFeature);
  lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);
  lifecycle->OnPromoEnded(CloseReason::kAction, true);
  EXPECT_CALL(tracker_, Dismissed(testing::Ref(kTestIPHFeature)));
}

TEST_F(FeaturePromoLifecycleTest, ClosePromoBubbleAndContinue_kLegalNotice) {
  set_promo_type(PromoType::kTutorial);
  set_promo_subtype(PromoSubtype::kLegalNotice);
  auto lifecycle = CreateLifecycle(kTestIPHFeature);
  lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);
  lifecycle->OnPromoEnded(CloseReason::kAction, true);
  auto promo_data = storage_service_.ReadPromoData(kTestIPHFeature);
  EXPECT_TRUE(promo_data->is_dismissed);
  EXPECT_EQ(CloseReason::kAction, promo_data->last_dismissed_by);

  EXPECT_CALL(tracker_, Dismissed(testing::Ref(kTestIPHFeature)));
  lifecycle->OnContinuedPromoEnded(false);
  CheckShownMetrics(lifecycle.get(), /*shown_count=*/1);
  EXPECT_CALL(tracker_, Dismissed).Times(0);
  promo_data = storage_service_.ReadPromoData(kTestIPHFeature);
  EXPECT_TRUE(promo_data->is_dismissed);
  EXPECT_EQ(CloseReason::kAction, promo_data->last_dismissed_by);
  EXPECT_EQ(0, promo_data->snooze_count);
  EXPECT_EQ(1, promo_data->show_count);
}

TEST_F(FeaturePromoLifecycleTest, ClosePromoBubbleAndContinue_kKeyedNotice) {
  set_promo_type(PromoType::kTutorial);
  set_promo_subtype(PromoSubtype::kKeyedNotice);
  auto lifecycle = CreateLifecycle(kTestIPHFeature);
  lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);
  lifecycle->OnPromoEnded(CloseReason::kAction, true);
  auto promo_data = storage_service_.ReadPromoData(kTestIPHFeature);
  EXPECT_TRUE(promo_data->is_dismissed);
  EXPECT_EQ(CloseReason::kAction, promo_data->last_dismissed_by);

  EXPECT_CALL(tracker_, Dismissed(testing::Ref(kTestIPHFeature)));
  lifecycle->OnContinuedPromoEnded(false);
  CheckShownMetrics(lifecycle.get(), /*shown_count=*/1);
  EXPECT_CALL(tracker_, Dismissed).Times(0);
  promo_data = storage_service_.ReadPromoData(kTestIPHFeature);
  EXPECT_TRUE(promo_data->is_dismissed);
  EXPECT_EQ(CloseReason::kAction, promo_data->last_dismissed_by);
  EXPECT_EQ(0, promo_data->snooze_count);
  EXPECT_EQ(1, promo_data->show_count);
}

TEST_F(FeaturePromoLifecycleTest, RotatingPromoGetIndex) {
  set_promo_type(PromoType::kRotating);

  // The tracker will be dismissed every time the promo is ended.
  EXPECT_CALL(tracker_, Dismissed).Times(4);

  auto lifecycle = CreateLifecycle(kTestIPHFeature);
  EXPECT_EQ(0, lifecycle->GetPromoIndex());
  EXPECT_TRUE(lifecycle->CanShow());
  lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);
  lifecycle->OnPromoEnded(CloseReason::kDismiss, false);
  auto promo_data = storage_service_.ReadPromoData(kTestIPHFeature);
  EXPECT_EQ(1, promo_data->promo_index);

  lifecycle = CreateLifecycle(kTestIPHFeature);
  EXPECT_EQ(1, lifecycle->GetPromoIndex());
  lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);
  lifecycle->OnPromoEnded(CloseReason::kDismiss, false);
  promo_data = storage_service_.ReadPromoData(kTestIPHFeature);
  EXPECT_EQ(2, promo_data->promo_index);

  lifecycle = CreateLifecycle(kTestIPHFeature);
  EXPECT_EQ(2, lifecycle->GetPromoIndex());
  EXPECT_TRUE(lifecycle->CanShow());
  lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);
  lifecycle->OnPromoEnded(CloseReason::kDismiss, false);
  promo_data = storage_service_.ReadPromoData(kTestIPHFeature);
  EXPECT_EQ(3, promo_data->promo_index);

  lifecycle = CreateLifecycle(kTestIPHFeature);
  EXPECT_EQ(0, lifecycle->GetPromoIndex());
  lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);
  lifecycle->OnPromoEnded(CloseReason::kDismiss, false);
  promo_data = storage_service_.ReadPromoData(kTestIPHFeature);
  EXPECT_EQ(1, promo_data->promo_index);

  CheckShownMetrics(lifecycle.get(), 4, {{0, 2}, {1, 1}, {2, 1}});
}

TEST_F(FeaturePromoLifecycleTest, RotatingPromoSetIndex) {
  set_promo_type(PromoType::kRotating);

  // The tracker will be dismissed every time the promo is ended.
  EXPECT_CALL(tracker_, Dismissed).Times(1);

  // Load the lifecycle but skip to promo at index 2.
  auto lifecycle = CreateLifecycle(kTestIPHFeature);
  EXPECT_EQ(0, lifecycle->GetPromoIndex());
  lifecycle->SetPromoIndex(2);
  EXPECT_EQ(2, lifecycle->GetPromoIndex());
  lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);
  lifecycle->OnPromoEnded(CloseReason::kDismiss, false);
  auto promo_data = storage_service_.ReadPromoData(kTestIPHFeature);
  EXPECT_EQ(3, promo_data->promo_index);
}

TEST_F(FeaturePromoLifecycleTest, ReshowAfterDelay) {
  auto& clock = UseTestClock();
  set_promo_type(PromoType::kToast);
  set_promo_subtype(PromoSubtype::kLegalNotice);
  set_reshow_params(base::Minutes(5), std::nullopt);

  EXPECT_CALL(tracker_, Dismissed);
  auto lifecycle = CreateLifecycle(kTestIPHFeature);
  EXPECT_TRUE(lifecycle->CanShow());
  lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);
  lifecycle->OnPromoEnded(CloseReason::kDismiss, false);

  // Try reshowing several times, first inside the reshow delay period, then
  // outside of it.
  for (int i = 0; i < 3; ++i) {
    clock.Advance(base::Minutes(3));
    lifecycle = CreateLifecycle(kTestIPHFeature);
    EXPECT_EQ(FeaturePromoResult::kBlockedByReshowDelay, lifecycle->CanShow());

    EXPECT_CALL(tracker_, Dismissed);
    clock.Advance(base::Minutes(3));
    EXPECT_TRUE(lifecycle->CanShow());
    lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);
    lifecycle->OnPromoEnded(CloseReason::kDismiss, false);
  }
}

TEST_F(FeaturePromoLifecycleTest, ReshowAfterDelayWithLimit) {
  auto& clock = UseTestClock();
  set_promo_type(PromoType::kToast);
  set_promo_subtype(PromoSubtype::kLegalNotice);
  set_reshow_params(base::Minutes(5), 3);

  EXPECT_CALL(tracker_, Dismissed);
  auto lifecycle = CreateLifecycle(kTestIPHFeature);
  EXPECT_TRUE(lifecycle->CanShow());
  lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);
  lifecycle->OnPromoEnded(CloseReason::kDismiss, false);

  // Try reshowing several times, first inside the reshow delay period, then
  // outside of it.
  for (int i = 0; i < 2; ++i) {
    clock.Advance(base::Minutes(3));
    lifecycle = CreateLifecycle(kTestIPHFeature);
    EXPECT_EQ(FeaturePromoResult::kBlockedByReshowDelay, lifecycle->CanShow());

    EXPECT_CALL(tracker_, Dismissed);
    clock.Advance(base::Minutes(3));
    EXPECT_TRUE(lifecycle->CanShow());
    lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);
    lifecycle->OnPromoEnded(CloseReason::kDismiss, false);
  }

  // At this point, the limit should have been reached, so no amount of delay
  // will re-enable the promo.
  clock.Advance(base::Minutes(3));
  lifecycle = CreateLifecycle(kTestIPHFeature);
  EXPECT_EQ(FeaturePromoResult::kPermanentlyDismissed, lifecycle->CanShow());

  clock.Advance(base::Minutes(3));
  lifecycle = CreateLifecycle(kTestIPHFeature);
  EXPECT_EQ(FeaturePromoResult::kPermanentlyDismissed, lifecycle->CanShow());
}

template <typename... Args>
class FeaturePromoLifecycleParamTest
    : public FeaturePromoLifecycleTest,
      public testing::WithParamInterface<
          std::tuple<PromoType, PromoSubtype, Args...>> {
 public:
  FeaturePromoLifecycleParamTest() = default;
  ~FeaturePromoLifecycleParamTest() override = default;

  using ValueType = std::tuple<PromoType, PromoSubtype, Args...>;

  template <typename T>
  T GetParamT() const {
    return std::get<T>(testing::WithParamInterface<ValueType>::GetParam());
  }

  void SetUp() override {
    set_promo_type(GetParamT<PromoType>());
    set_promo_subtype(GetParamT<PromoSubtype>());
    FeaturePromoLifecycleTest::SetUp();
  }

  std::unique_ptr<FeaturePromoLifecycle> CreateLifecycle(
      const base::Feature& feature,
      const char* app_id = nullptr) {
    if (!app_id) {
      app_id = promo_subtype() == PromoSubtype::kKeyedNotice ? kAppName : "";
    }
    return MaybeSetReshowParams(std::make_unique<FeaturePromoLifecycle>(
        &storage_service_, app_id, &feature, promo_type(), promo_subtype(),
        promo_type() == PromoType::kRotating ? kNumRotatingPromos : 0));
  }
};

using FeaturePromoLifecycleWriteDataTest =
    FeaturePromoLifecycleParamTest<CloseReason>;

INSTANTIATE_TEST_SUITE_P(
    ,
    FeaturePromoLifecycleWriteDataTest,
    testing::Combine(testing::Values(PromoType::kTutorial),
                     testing::Values(PromoSubtype::kNormal,
                                     PromoSubtype::kKeyedNotice,
                                     PromoSubtype::kActionableAlert,
                                     PromoSubtype::kLegalNotice),
                     testing::Values(CloseReason::kDismiss,
                                     CloseReason::kSnooze,
                                     CloseReason::kAction,
                                     CloseReason::kCancel,
                                     CloseReason::kTimeout,
                                     CloseReason::kAbortPromo,
                                     CloseReason::kFeatureEngaged)),
    (ParamToString<PromoType, PromoSubtype, CloseReason>));

TEST_P(FeaturePromoLifecycleWriteDataTest, DoesDemoWriteData) {
  auto lifecycle = CreateLifecycle(kTestIPHFeature);
  lifecycle->OnPromoShownForDemo(CreateHelpBubble());
  lifecycle->OnPromoEnded(GetParamT<CloseReason>());
  ASSERT_FALSE(storage_service_.ReadPromoData(kTestIPHFeature).has_value());
}

TEST_P(FeaturePromoLifecycleWriteDataTest, DataWrittenAndTrackerDismissed) {
  auto lifecycle = CreateLifecycle(kTestIPHFeature);
  lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);

  const auto close_reason = GetParamT<CloseReason>();
  EXPECT_CALL(tracker_, Dismissed(testing::Ref(kTestIPHFeature)));
  lifecycle->OnPromoEnded(close_reason);
  EXPECT_CALL(tracker_, Dismissed).Times(0);

  auto promo_data = storage_service_.ReadPromoData(kTestIPHFeature);
  EXPECT_EQ(1, promo_data->show_count);
  if (close_reason == CloseReason::kAbortPromo) {
    EXPECT_FALSE(promo_data->is_dismissed);
    EXPECT_EQ(0, promo_data->snooze_count);
  } else if (close_reason == CloseReason::kSnooze) {
    EXPECT_FALSE(promo_data->is_dismissed);
    EXPECT_EQ(1, promo_data->snooze_count);
    EXPECT_GE(promo_data->last_snooze_time, promo_data->last_show_time);
  } else {
    EXPECT_EQ(0, promo_data->snooze_count);
    EXPECT_TRUE(promo_data->is_dismissed);
    EXPECT_EQ(close_reason, promo_data->last_dismissed_by);
  }
}

TEST_P(FeaturePromoLifecycleWriteDataTest, FirstAndLastShowTimeUpdated) {
  auto lifecycle = CreateLifecycle(kTestIPHFeature);
  lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);

  EXPECT_CALL(tracker_, Dismissed(testing::Ref(kTestIPHFeature)));
  lifecycle->OnPromoEnded(FeaturePromoClosedReason::kAbortPromo);
  EXPECT_CALL(tracker_, Dismissed).Times(0);

  const auto old_data = storage_service_.ReadPromoData(kTestIPHFeature);
  EXPECT_EQ(1, old_data->show_count);
  EXPECT_EQ(old_data->first_show_time, old_data->last_show_time);

  task_environment_.FastForwardBy(base::Seconds(5));

  lifecycle = CreateLifecycle(kTestIPHFeature);
  lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);

  EXPECT_CALL(tracker_, Dismissed(testing::Ref(kTestIPHFeature)));
  lifecycle->OnPromoEnded(GetParamT<CloseReason>());
  EXPECT_CALL(tracker_, Dismissed).Times(0);

  const auto new_data = storage_service_.ReadPromoData(kTestIPHFeature);

  EXPECT_EQ(2, new_data->show_count);
  EXPECT_EQ(new_data->first_show_time, old_data->first_show_time);
  EXPECT_GT(new_data->last_show_time, old_data->last_show_time);
}

using FeaturePromoLifecycleTypesTest = FeaturePromoLifecycleParamTest<>;

INSTANTIATE_TEST_SUITE_P(
    ,
    FeaturePromoLifecycleTypesTest,
    testing::Combine(testing::Values(PromoType::kLegacy,
                                     PromoType::kToast,
                                     PromoType::kSnooze,
                                     PromoType::kTutorial,
                                     PromoType::kCustomAction),
                     testing::Values(PromoSubtype::kNormal,
                                     PromoSubtype::kKeyedNotice,
                                     PromoSubtype::kActionableAlert,
                                     PromoSubtype::kLegalNotice)),
    (ParamToString<PromoType, PromoSubtype>));

TEST_P(FeaturePromoLifecycleTypesTest, AllowFirstTimeIPH) {
  auto lifecycle = CreateLifecycle(kTestIPHFeature);
  EXPECT_TRUE(lifecycle->CanShow());
}

TEST_P(FeaturePromoLifecycleTypesTest, BlockDismissedIPH) {
  auto lifecycle = CreateLifecycle(kTestIPHFeature);
  lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);
  EXPECT_CALL(tracker_, Dismissed);
  lifecycle->OnPromoEnded(CloseReason::kDismiss);
  lifecycle = CreateLifecycle(kTestIPHFeature);
  EXPECT_EQ(FeaturePromoResult::kPermanentlyDismissed, lifecycle->CanShow());
  storage_service_.Reset(kTestIPHFeature);
  lifecycle = CreateLifecycle(kTestIPHFeature);
  EXPECT_TRUE(lifecycle->CanShow());
}

TEST_P(FeaturePromoLifecycleTypesTest, BlockSnoozedIPH) {
  auto lifecycle = CreateLifecycle(kTestIPHFeature);
  lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);
  EXPECT_CALL(tracker_, Dismissed);
  lifecycle->OnPromoEnded(CloseReason::kSnooze);
  lifecycle = CreateLifecycle(kTestIPHFeature);
  EXPECT_EQ(GetSnoozedResult(), lifecycle->CanShow());
  storage_service_.Reset(kTestIPHFeature);
  lifecycle = CreateLifecycle(kTestIPHFeature);
  EXPECT_TRUE(lifecycle->CanShow());
}

TEST_P(FeaturePromoLifecycleTypesTest, ReleaseSnoozedIPH) {
  auto lifecycle = CreateLifecycle(kTestIPHFeature);
  lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);
  EXPECT_CALL(tracker_, Dismissed);
  lifecycle->OnPromoEnded(CloseReason::kSnooze);
  lifecycle = CreateLifecycle(kTestIPHFeature);
  EXPECT_EQ(GetSnoozedResult(), lifecycle->CanShow());
  task_environment_.FastForwardBy(features::GetSnoozeDuration());
  lifecycle = CreateLifecycle(kTestIPHFeature);
  EXPECT_TRUE(lifecycle->CanShow());
}

TEST_P(FeaturePromoLifecycleTypesTest, MultipleIPH) {
  auto lifecycle = CreateLifecycle(kTestIPHFeature);
  lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);
  EXPECT_CALL(tracker_, Dismissed);
  lifecycle->OnPromoEnded(CloseReason::kSnooze);

  task_environment_.FastForwardBy(base::Hours(1));

  lifecycle = CreateLifecycle(kTestIPHFeature2);
  lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);
  EXPECT_CALL(tracker_, Dismissed);
  lifecycle->OnPromoEnded(CloseReason::kSnooze);

  lifecycle = CreateLifecycle(kTestIPHFeature);
  EXPECT_EQ(GetSnoozedResult(), lifecycle->CanShow());

  lifecycle = CreateLifecycle(kTestIPHFeature2);
  EXPECT_EQ(GetSnoozedResult(), lifecycle->CanShow());

  task_environment_.FastForwardBy(features::GetSnoozeDuration() -
                                  base::Hours(1));

  lifecycle = CreateLifecycle(kTestIPHFeature);
  EXPECT_TRUE(lifecycle->CanShow());

  lifecycle = CreateLifecycle(kTestIPHFeature2);
  EXPECT_EQ(GetSnoozedResult(), lifecycle->CanShow());

  task_environment_.FastForwardBy(base::Hours(1));

  lifecycle = CreateLifecycle(kTestIPHFeature2);
  EXPECT_TRUE(lifecycle->CanShow());
}

TEST_P(FeaturePromoLifecycleTypesTest, SnoozeNonInteractedIPH) {
  auto lifecycle = CreateLifecycle(kTestIPHFeature);
  lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);
  EXPECT_CALL(tracker_, Dismissed);
  lifecycle.reset();

  lifecycle = CreateLifecycle(kTestIPHFeature);
  EXPECT_EQ(GetNonInteractedResult(), lifecycle->CanShow());

  task_environment_.FastForwardBy(features::GetSnoozeDuration());

  lifecycle = CreateLifecycle(kTestIPHFeature);
  EXPECT_TRUE(lifecycle->CanShow());
}

TEST_P(FeaturePromoLifecycleTypesTest, NewProfile) {
  // Set the new profile grace period end into the future by making the profile
  // very new.
  storage_service_.set_profile_creation_time_for_testing(
      storage_service_.GetCurrentTime() - base::Hours(12));
  auto lifecycle = CreateLifecycle(kTestIPHFeature);
  EXPECT_EQ(GetNewProfileResult(), lifecycle->CanShow());
}

TEST_P(FeaturePromoLifecycleTypesTest, MaxShowCountReached) {
  FeaturePromoData data;
  data.show_count = features::GetMaxPromoShowCount();
  const auto abort_cooldown = features::GetAbortCooldown();
  data.first_show_time = base::Time::Now() - abort_cooldown * 3;
  data.last_show_time = base::Time::Now() - abort_cooldown * 2;
  storage_service_.SavePromoData(kTestIPHFeature, data);
  auto lifecycle = CreateLifecycle(kTestIPHFeature);
  EXPECT_EQ(GetExceededResult(), lifecycle->CanShow());
}

TEST_P(FeaturePromoLifecycleTypesTest, MaxShowCountNotReachedDueToSnooze) {
  FeaturePromoData data;
  data.show_count = features::GetMaxPromoShowCount();
  data.snooze_count = 1;
  const auto abort_cooldown = features::GetAbortCooldown();
  data.first_show_time = base::Time::Now() - abort_cooldown * 3;
  data.last_show_time = base::Time::Now() - abort_cooldown * 2;
  storage_service_.SavePromoData(kTestIPHFeature, data);
  auto lifecycle = CreateLifecycle(kTestIPHFeature);
  EXPECT_EQ(FeaturePromoResult::Success(), lifecycle->CanShow());
}

TEST_P(FeaturePromoLifecycleTypesTest, MaxShowCountReachedAfterSnooze) {
  FeaturePromoData data;
  data.show_count = features::GetMaxPromoShowCount() + 2;
  data.snooze_count = 2;
  const auto abort_cooldown = features::GetAbortCooldown();
  data.first_show_time = base::Time::Now() - abort_cooldown * 3;
  data.last_show_time = base::Time::Now() - abort_cooldown * 2;
  storage_service_.SavePromoData(kTestIPHFeature, data);
  auto lifecycle = CreateLifecycle(kTestIPHFeature);
  EXPECT_EQ(GetExceededResult(), lifecycle->CanShow());
}

using FeaturePromoLifecycleAppTest = FeaturePromoLifecycleParamTest<>;

INSTANTIATE_TEST_SUITE_P(
    ,
    FeaturePromoLifecycleAppTest,
    testing::Combine(testing::Values(PromoType::kLegacy,
                                     PromoType::kToast,
                                     PromoType::kTutorial,
                                     PromoType::kCustomAction),
                     testing::Values(PromoSubtype::kKeyedNotice)),
    (ParamToString<PromoType, PromoSubtype>));

TEST_P(FeaturePromoLifecycleAppTest, IPHBlockedKeyedNotice) {
  // Show and confirm for one app.
  auto lifecycle = CreateLifecycle(kTestIPHFeature, kAppName);
  lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);
  EXPECT_CALL(tracker_, Dismissed);
  lifecycle->OnPromoEnded(CloseReason::kDismiss);

  // That app should no longer allow showing.
  lifecycle = CreateLifecycle(kTestIPHFeature, kAppName);
  EXPECT_EQ(FeaturePromoResult::kPermanentlyDismissed, lifecycle->CanShow());

  // However a different app should be allowed.
  lifecycle = CreateLifecycle(kTestIPHFeature, kAppName2);
  EXPECT_TRUE(lifecycle->CanShow());

  // Show and dismiss in the other app.
  lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);
  EXPECT_CALL(tracker_, Dismissed);
  lifecycle->OnPromoEnded(CloseReason::kDismiss);

  // Now both apps should be blocked.
  lifecycle = CreateLifecycle(kTestIPHFeature, kAppName);
  EXPECT_EQ(FeaturePromoResult::kPermanentlyDismissed, lifecycle->CanShow());
  lifecycle = CreateLifecycle(kTestIPHFeature, kAppName2);
  EXPECT_EQ(FeaturePromoResult::kPermanentlyDismissed, lifecycle->CanShow());

  // But a different IPH should not be blocked.
  lifecycle = CreateLifecycle(kTestIPHFeature2, kAppName);
  EXPECT_TRUE(lifecycle->CanShow());
}

TEST_P(FeaturePromoLifecycleAppTest, ReshowAfterDelay) {
  set_reshow_params(base::Minutes(5), std::nullopt);
  auto& clock = UseTestClock();

  auto lifecycle = CreateLifecycle(kTestIPHFeature, kAppName);
  EXPECT_TRUE(lifecycle->CanShow());
  lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);
  EXPECT_CALL(tracker_, Dismissed);
  lifecycle->OnPromoEnded(CloseReason::kDismiss);

  // After a short time, the same promo cannot reshow, but it can show for a
  // different app.
  clock.Advance(base::Minutes(3));
  lifecycle = CreateLifecycle(kTestIPHFeature, kAppName);
  EXPECT_EQ(FeaturePromoResult::kBlockedByReshowDelay, lifecycle->CanShow());

  lifecycle = CreateLifecycle(kTestIPHFeature, kAppName2);
  EXPECT_TRUE(lifecycle->CanShow());
  lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);
  EXPECT_CALL(tracker_, Dismissed);
  lifecycle->OnPromoEnded(CloseReason::kDismiss);

  // After another short time, the first promo can show, but the second cannot
  // show yet.
  clock.Advance(base::Minutes(3));
  lifecycle = CreateLifecycle(kTestIPHFeature, kAppName);
  EXPECT_TRUE(lifecycle->CanShow());
  lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);
  EXPECT_CALL(tracker_, Dismissed);
  lifecycle->OnPromoEnded(CloseReason::kDismiss);

  lifecycle = CreateLifecycle(kTestIPHFeature, kAppName2);
  EXPECT_EQ(FeaturePromoResult::kBlockedByReshowDelay, lifecycle->CanShow());
}

TEST_P(FeaturePromoLifecycleAppTest, ReshowAfterDelayWithLimit) {
  set_reshow_params(base::Minutes(5), 2);
  auto& clock = UseTestClock();

  auto lifecycle = CreateLifecycle(kTestIPHFeature, kAppName);
  EXPECT_TRUE(lifecycle->CanShow());
  lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);
  EXPECT_CALL(tracker_, Dismissed);
  lifecycle->OnPromoEnded(CloseReason::kDismiss);

  // After a short time, the same promo cannot reshow, but it can show for a
  // different app.
  clock.Advance(base::Minutes(3));
  lifecycle = CreateLifecycle(kTestIPHFeature, kAppName);
  EXPECT_EQ(FeaturePromoResult::kBlockedByReshowDelay, lifecycle->CanShow());

  lifecycle = CreateLifecycle(kTestIPHFeature, kAppName2);
  EXPECT_TRUE(lifecycle->CanShow());
  lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);
  EXPECT_CALL(tracker_, Dismissed);
  lifecycle->OnPromoEnded(CloseReason::kDismiss);

  // After another short time, the first promo can show, but the second cannot
  // show yet.
  clock.Advance(base::Minutes(3));
  lifecycle = CreateLifecycle(kTestIPHFeature, kAppName);
  EXPECT_TRUE(lifecycle->CanShow());
  lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);
  EXPECT_CALL(tracker_, Dismissed);
  lifecycle->OnPromoEnded(CloseReason::kDismiss);

  lifecycle = CreateLifecycle(kTestIPHFeature, kAppName2);
  EXPECT_EQ(FeaturePromoResult::kBlockedByReshowDelay, lifecycle->CanShow());

  // Delay a bit longer, and ensure that the first app (which has been shown
  // twice) cannot reshow, but the second can.
  clock.Advance(base::Minutes(10));
  lifecycle = CreateLifecycle(kTestIPHFeature, kAppName);
  EXPECT_EQ(FeaturePromoResult::kPermanentlyDismissed, lifecycle->CanShow());

  lifecycle = CreateLifecycle(kTestIPHFeature, kAppName2);
  EXPECT_TRUE(lifecycle->CanShow());
}

}  // namespace user_education
