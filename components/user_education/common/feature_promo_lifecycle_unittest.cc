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
#include "base/test/task_environment.h"
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

  FeaturePromoResult GetSnoozedResult() const {
    return promo_subtype() == PromoSubtype::kNormal &&
                   (promo_type() == PromoType::kSnooze ||
                    promo_type() == PromoType::kTutorial ||
                    promo_type() == PromoType::kCustomAction)
               ? FeaturePromoResult::kSnoozed
               : FeaturePromoResult::Success();
  }

  std::unique_ptr<FeaturePromoLifecycle> CreateLifecycle(
      const base::Feature& feature,
      const char* app_id = nullptr) {
    if (!app_id) {
      app_id = promo_subtype() == PromoSubtype::kPerApp ? kAppName : "";
    }
    return std::make_unique<FeaturePromoLifecycle>(
        &storage_service_, app_id, &feature, promo_type(), promo_subtype());
  }

  std::unique_ptr<test::TestHelpBubble> CreateHelpBubble() {
    ++num_open_bubbles_;
    auto result =
        std::make_unique<test::TestHelpBubble>(&element_, HelpBubbleParams());
    help_bubble_subscriptions_.emplace_back(
        result->AddOnCloseCallback(base::BindLambdaForTesting(
            [this](HelpBubble*) { --num_open_bubbles_; })));
    return result;
  }

  auto CheckShownMetrics(const std::unique_ptr<FeaturePromoLifecycle> lifecycle,
                         int shown_count) {
    EXPECT_EQ(shown_count,
              user_action_tester_.GetActionCount("UserEducation.MessageShown"));
    EXPECT_EQ(
        shown_count,
        user_action_tester_.GetActionCount(base::StringPrintf(
            "UserEducation.MessageShown.%s", lifecycle->iph_feature()->name)));

    std::string name = "";
    switch (lifecycle->promo_subtype()) {
      case PromoSubtype::kPerApp:
        name = "PerApp.";
        break;
      case PromoSubtype::kLegalNotice:
        name = "LegalNotice.";
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
      case PromoType::kUnspecified:
        NOTREACHED();
        return;
    }

    EXPECT_EQ(shown_count,
              user_action_tester_.GetActionCount(base::StringPrintf(
                  "UserEducation.MessageShown.%s", name.data())));
    histogram_tester_.ExpectBucketCount(
        "UserEducation.MessageShown.Type",
        static_cast<int>(lifecycle->promo_type()), shown_count);
    histogram_tester_.ExpectBucketCount(
        "UserEducation.MessageShown.SubType",
        static_cast<int>(lifecycle->promo_subtype()), shown_count);
  }

 protected:
  PromoType promo_type_ = PromoType::kSnooze;
  PromoSubtype promo_subtype_ = PromoSubtype::kNormal;
  int num_open_bubbles_ = 0;
  ui::test::TestElement element_{kTestElementId, kTestElementContext};
  test::TestFeaturePromoStorageService storage_service_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  testing::StrictMock<feature_engagement::test::MockTracker> tracker_;
  std::vector<base::CallbackListSubscription> help_bubble_subscriptions_;
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;
};

TEST_F(FeaturePromoLifecycleTest, BubbleClosedOnDiscard) {
  auto lifecycle = CreateLifecycle(kTestIPHFeature);
  lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);
  EXPECT_CALL(tracker_, Dismissed(testing::Ref(kTestIPHFeature)));
  CheckShownMetrics(std::move(lifecycle), /*shown_count=*/1);
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
  CheckShownMetrics(std::move(lifecycle), /*shown_count=*/1);
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
  CheckShownMetrics(std::move(lifecycle), /*shown_count=*/1);
  EXPECT_CALL(tracker_, Dismissed).Times(0);
  promo_data = storage_service_.ReadPromoData(kTestIPHFeature);
  EXPECT_TRUE(promo_data->is_dismissed);
  EXPECT_EQ(CloseReason::kAction, promo_data->last_dismissed_by);
  EXPECT_EQ(0, promo_data->snooze_count);
  EXPECT_EQ(1, promo_data->show_count);
}

TEST_F(FeaturePromoLifecycleTest, ClosePromoBubbleAndContinue_kPerApp) {
  set_promo_type(PromoType::kTutorial);
  set_promo_subtype(PromoSubtype::kPerApp);
  auto lifecycle = CreateLifecycle(kTestIPHFeature);
  lifecycle->OnPromoShown(CreateHelpBubble(), &tracker_);
  lifecycle->OnPromoEnded(CloseReason::kAction, true);
  auto promo_data = storage_service_.ReadPromoData(kTestIPHFeature);
  EXPECT_TRUE(promo_data->is_dismissed);
  EXPECT_EQ(CloseReason::kAction, promo_data->last_dismissed_by);

  EXPECT_CALL(tracker_, Dismissed(testing::Ref(kTestIPHFeature)));
  lifecycle->OnContinuedPromoEnded(false);
  CheckShownMetrics(std::move(lifecycle), /*shown_count=*/1);
  EXPECT_CALL(tracker_, Dismissed).Times(0);
  promo_data = storage_service_.ReadPromoData(kTestIPHFeature);
  EXPECT_TRUE(promo_data->is_dismissed);
  EXPECT_EQ(CloseReason::kAction, promo_data->last_dismissed_by);
  EXPECT_EQ(0, promo_data->snooze_count);
  EXPECT_EQ(1, promo_data->show_count);
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
      app_id = promo_subtype() == PromoSubtype::kPerApp ? kAppName : "";
    }
    return std::make_unique<FeaturePromoLifecycle>(
        &storage_service_, app_id, &feature, promo_type(), promo_subtype());
  }
};

using FeaturePromoLifecycleWriteDataTest =
    FeaturePromoLifecycleParamTest<CloseReason>;

INSTANTIATE_TEST_SUITE_P(
    ,
    FeaturePromoLifecycleWriteDataTest,
    testing::Combine(testing::Values(PromoType::kTutorial),
                     testing::Values(PromoSubtype::kNormal,
                                     PromoSubtype::kPerApp,
                                     PromoSubtype::kLegalNotice),
                     testing::Values(CloseReason::kDismiss,
                                     CloseReason::kSnooze,
                                     CloseReason::kAction,
                                     CloseReason::kCancel,
                                     CloseReason::kTimeout,
                                     CloseReason::kAbortPromo,
                                     CloseReason::kFeatureEngaged)),
    (ParamToString<PromoType, PromoSubtype, CloseReason>));

TEST_P(FeaturePromoLifecycleWriteDataTest, DemoDoesNotWriteData) {
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
                                     PromoSubtype::kPerApp,
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
  const auto expect_can_show = (promo_subtype() == PromoSubtype::kNormal &&
                                (promo_type() == PromoType::kLegacy ||
                                 promo_type() == PromoType::kToast))
                                   ? FeaturePromoResult::Success()
                                   : FeaturePromoResult::kPermanentlyDismissed;
  EXPECT_EQ(expect_can_show, lifecycle->CanShow());
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
  EXPECT_EQ(GetSnoozedResult(), lifecycle->CanShow());

  task_environment_.FastForwardBy(features::GetSnoozeDuration());

  lifecycle = CreateLifecycle(kTestIPHFeature);
  EXPECT_TRUE(lifecycle->CanShow());
}

using FeaturePromoLifecycleAppTest = FeaturePromoLifecycleParamTest<>;

INSTANTIATE_TEST_SUITE_P(
    ,
    FeaturePromoLifecycleAppTest,
    testing::Combine(testing::Values(PromoType::kLegacy,
                                     PromoType::kToast,
                                     PromoType::kTutorial,
                                     PromoType::kCustomAction),
                     testing::Values(PromoSubtype::kPerApp)),
    (ParamToString<PromoType, PromoSubtype>));

TEST_P(FeaturePromoLifecycleAppTest, IPHBlockedPerApp) {
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

}  // namespace user_education
