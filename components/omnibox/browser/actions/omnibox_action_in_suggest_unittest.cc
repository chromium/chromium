// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/omnibox_action_in_suggest.h"

#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/omnibox/browser/actions/omnibox_pedal_provider.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/entity_info.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

using omnibox::ActionInfo;
using ActionType = omnibox::ActionInfo_ActionType;

namespace {
class FakeOmniboxAction : public OmniboxAction {
 public:
  explicit FakeOmniboxAction(OmniboxActionId id)
      : OmniboxAction(LabelStrings(u"", u"", u"", u""), GURL{}), id_(id) {}
  OmniboxActionId ActionId() const override { return id_; }

 private:
  ~FakeOmniboxAction() override = default;
  OmniboxActionId id_{};
};

// Note: can't use operator<<, because ActionType is a plain old enum.
const char* ToString(ActionType type) {
  switch (type) {
    case omnibox::ActionInfo_ActionType_CALL:
      return "Call";
    case omnibox::ActionInfo_ActionType_DIRECTIONS:
      return "Directions";
    case omnibox::ActionInfo_ActionType_REVIEWS:
      return "Reviews";
    default:
      NOTREACHED_IN_MIGRATION();
  }
}
}  // namespace

class OmniboxActionInSuggestTest : public testing::Test {
 public:
  OmniboxActionInSuggestTest();

 protected:
  MockAutocompleteProviderClient client_;
  OmniboxAction::ExecutionContext context_;
};

OmniboxActionInSuggestTest::OmniboxActionInSuggestTest()
    : context_(client_,
               OmniboxAction::ExecutionContext::OpenUrlCallback(),
               base::TimeTicks(),
               WindowOpenDisposition::IGNORE_ACTION) {}

TEST_F(OmniboxActionInSuggestTest, CheckLabelsArePresentForKnownTypes) {
  struct {
    ActionType action_type;
    int want_hint;
    int want_contents;
    int want_accessibility_focus_hint;
    int want_accessibility_activate_hint;
  } test_cases[] = {{
                        omnibox::ActionInfo_ActionType_CALL,
                        IDS_OMNIBOX_ACTION_IN_SUGGEST_CALL_HINT,
                        IDS_OMNIBOX_ACTION_IN_SUGGEST_CALL_CONTENTS,
                        IDS_ACC_OMNIBOX_ACTION_IN_SUGGEST_SUFFIX,
                        IDS_OMNIBOX_ACTION_IN_SUGGEST_CALL_CONTENTS,
                    },
                    {
                        omnibox::ActionInfo_ActionType_DIRECTIONS,
                        IDS_OMNIBOX_ACTION_IN_SUGGEST_DIRECTIONS_HINT,
                        IDS_OMNIBOX_ACTION_IN_SUGGEST_DIRECTIONS_CONTENTS,
                        IDS_ACC_OMNIBOX_ACTION_IN_SUGGEST_SUFFIX,
                        IDS_OMNIBOX_ACTION_IN_SUGGEST_DIRECTIONS_CONTENTS,
                    },
                    {
                        omnibox::ActionInfo_ActionType_REVIEWS,
                        IDS_OMNIBOX_ACTION_IN_SUGGEST_REVIEWS_HINT,
                        IDS_OMNIBOX_ACTION_IN_SUGGEST_REVIEWS_CONTENTS,
                        IDS_ACC_OMNIBOX_ACTION_IN_SUGGEST_SUFFIX,
                        IDS_OMNIBOX_ACTION_IN_SUGGEST_REVIEWS_CONTENTS,
                    }};

  for (const auto& test_case : test_cases) {
    ActionInfo action_info;
    action_info.set_action_type(test_case.action_type);

    auto action = base::MakeRefCounted<OmniboxActionInSuggest>(
        std::move(action_info), std::nullopt);
    EXPECT_EQ(OmniboxActionId::ACTION_IN_SUGGEST, action->ActionId())
        << "while evaluatin action " << ToString(test_case.action_type);
    EXPECT_EQ(test_case.action_type, action->Type());

    const auto& labels = action->GetLabelStrings();
    EXPECT_EQ(labels.hint, l10n_util::GetStringUTF16(test_case.want_hint))
        << "while evaluatin action " << ToString(test_case.action_type);
    EXPECT_EQ(labels.suggestion_contents,
              l10n_util::GetStringUTF16(test_case.want_contents))
        << "while evaluatin action " << ToString(test_case.action_type);
    EXPECT_EQ(
        labels.accessibility_suffix,
        l10n_util::GetStringUTF16(test_case.want_accessibility_focus_hint))
        << "while evaluatin action " << ToString(test_case.action_type);
    EXPECT_EQ(
        labels.accessibility_hint,
        l10n_util::GetStringUTF16(test_case.want_accessibility_activate_hint))
        << "while evaluatin action " << ToString(test_case.action_type);
  }
}

TEST_F(OmniboxActionInSuggestTest, ConversionFromAction) {
  const ActionType test_cases[]{omnibox::ActionInfo_ActionType_CALL,
                                omnibox::ActionInfo_ActionType_DIRECTIONS,
                                omnibox::ActionInfo_ActionType_REVIEWS};

  for (auto test_case : test_cases) {
    ActionInfo action_info;
    action_info.set_action_type(test_case);

    scoped_refptr<OmniboxAction> upcasted_action =
        base::MakeRefCounted<OmniboxActionInSuggest>(std::move(action_info),
                                                     std::nullopt);

    auto* downcasted_action =
        OmniboxActionInSuggest::FromAction(upcasted_action.get());

    EXPECT_EQ(upcasted_action.get(), downcasted_action)
        << "while evaluatin action " << ToString(test_case);
    EXPECT_EQ(test_case, downcasted_action->Type())
        << "while evaluatin action " << ToString(test_case);
  }
}

TEST_F(OmniboxActionInSuggestTest, ConversionFromNonAction) {
  const OmniboxActionId test_cases[]{OmniboxActionId::HISTORY_CLUSTERS,
                                     OmniboxActionId::PEDAL,
                                     OmniboxActionId::TAB_SWITCH};

  for (auto test_case : test_cases) {
    auto fake_action = base::MakeRefCounted<FakeOmniboxAction>(test_case);
    EXPECT_EQ(nullptr, OmniboxActionInSuggest::FromAction(fake_action.get()));
  }
}

TEST_F(OmniboxActionInSuggestTest, AllDeclaredActionTypesAreProperlyReflected) {
  // This test verifies that we're not quietly migrating new action types, and
  // failing to recognize the need for appropriate coverage, both in terms of
  // labels (hints, accessibility) but also UMA metrics.
  for (int type = ActionInfo::ActionType_MIN;
       type <= ActionInfo::ActionType_MAX; type++) {
    if (omnibox::ActionInfo_ActionType_IsValid(type)) {
      ActionInfo action_info;
      action_info.set_action_type(ActionType(type));

      // This is a valid action type. Object MUST build.
      auto action = base::MakeRefCounted<OmniboxActionInSuggest>(
          std::move(action_info), std::nullopt);
      // This is a valid action type. Object MUST be able to report metrics.
      {
        base::HistogramTester histograms;
        action->RecordActionShown(1, false);
        // NO actions report to the 'Unknown' bucket.
        histograms.ExpectBucketCount("Omnibox.ActionInSuggest.Shown", 0, 0);
        histograms.ExpectBucketCount("Omnibox.ActionInSuggest.Used", 0, 0);
        histograms.ExpectTotalCount("Omnibox.ActionInSuggest.Shown", 1);
        histograms.ExpectTotalCount("Omnibox.ActionInSuggest.Used", 0);
      }

      {
        base::HistogramTester histograms;
        action->RecordActionShown(1, true);
        // NO actions report to the 'Unknown' bucket.
        histograms.ExpectBucketCount("Omnibox.ActionInSuggest.Shown", 0, 0);
        histograms.ExpectBucketCount("Omnibox.ActionInSuggest.Used", 0, 0);
        histograms.ExpectTotalCount("Omnibox.ActionInSuggest.Shown", 1);
        histograms.ExpectTotalCount("Omnibox.ActionInSuggest.Used", 1);
      }
    }
  }
}

TEST_F(OmniboxActionInSuggestTest, HistogramsRecording) {
  enum class UmaTypeForTest {
    kUnknown = 0,
    kCall,
    kDirections,
    kWebsite,
    kReviews,
  };

  // Correlation between ActionType and UMA-recorded bucket.
  struct {
    omnibox::ActionInfo::ActionType type;
    UmaTypeForTest recordedUmaType;
    const char* dedicatedHistogram;
  } test_cases[]{
      {omnibox::ActionInfo_ActionType_CALL, UmaTypeForTest::kCall,
       "Omnibox.ActionInSuggest.UsageByType.Call"},
      {omnibox::ActionInfo_ActionType_DIRECTIONS, UmaTypeForTest::kDirections,
       "Omnibox.ActionInSuggest.UsageByType.Directions"},
      {omnibox::ActionInfo_ActionType_REVIEWS, UmaTypeForTest::kReviews,
       "Omnibox.ActionInSuggest.UsageByType.Reviews"},
  };

  for (const auto& test_case : test_cases) {
    ActionInfo action_info;
    action_info.set_action_type(test_case.type);
    scoped_refptr<OmniboxAction> action =
        base::MakeRefCounted<OmniboxActionInSuggest>(std::move(action_info),
                                                     std::nullopt);

    {
      // Just show.
      base::HistogramTester histograms;
      action->RecordActionShown(1, false);
      histograms.ExpectBucketCount("Omnibox.ActionInSuggest.Shown",
                                   test_case.recordedUmaType, 1);
      histograms.ExpectBucketCount("Omnibox.ActionInSuggest.Used",
                                   test_case.recordedUmaType, 0);
      histograms.ExpectTotalCount("Omnibox.ActionInSuggest.Shown", 1);
      histograms.ExpectTotalCount("Omnibox.ActionInSuggest.Used", 0);

      histograms.ExpectBucketCount(test_case.dedicatedHistogram, false, 1);
      histograms.ExpectTotalCount(test_case.dedicatedHistogram, 1);
    }

    {
      // Show and execute.
      base::HistogramTester histograms;
      action->RecordActionShown(1, true);
      histograms.ExpectBucketCount("Omnibox.ActionInSuggest.Shown",
                                   test_case.recordedUmaType, 1);
      histograms.ExpectBucketCount("Omnibox.ActionInSuggest.Used",
                                   test_case.recordedUmaType, 1);
      histograms.ExpectTotalCount("Omnibox.ActionInSuggest.Shown", 1);
      histograms.ExpectTotalCount("Omnibox.ActionInSuggest.Used", 1);

      histograms.ExpectBucketCount(test_case.dedicatedHistogram, true, 1);
      histograms.ExpectTotalCount(test_case.dedicatedHistogram, 1);
    }
  }
}
