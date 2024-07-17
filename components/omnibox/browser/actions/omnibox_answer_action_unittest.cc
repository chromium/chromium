// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/omnibox_answer_action.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/answer_type.pb.h"
#include "third_party/omnibox_proto/rich_answer_template.pb.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

constexpr int kSuggestionAccessibilitySuffixId =
    IDS_ACC_OMNIBOX_ACTION_IN_SUGGEST_SUFFIX;
}  // namespace

class OmniboxAnswerActionTest : public testing::Test {
 protected:
  OmniboxAnswerActionTest() = default;
};

TEST_F(OmniboxAnswerActionTest, ActionHasLabelsFromEnhancement) {
  // Create RichAnswerTemplate with enhahcements.
  omnibox::RichAnswerTemplate answer_template;
  omnibox::SuggestionEnhancement* enhancement =
      answer_template.mutable_enhancements()->add_enhancements();
  std::string display_text = "Similar and opposite words";
  enhancement->set_display_text(display_text);
  auto action = base::MakeRefCounted<OmniboxAnswerAction>(
      std::move(*enhancement), TemplateURLRef::SearchTermsArgs(),
      omnibox::ANSWER_TYPE_DICTIONARY);
  const auto& labels = action->GetLabelStrings();

  // Ensure actions have the correct labels.
  EXPECT_EQ(base::UTF16ToUTF8(labels.hint), display_text);
  EXPECT_EQ(base::UTF16ToUTF8(labels.suggestion_contents), display_text);
  EXPECT_EQ(labels.accessibility_suffix,
            l10n_util::GetStringUTF16(kSuggestionAccessibilitySuffixId));
  EXPECT_EQ(base::UTF16ToUTF8(labels.accessibility_hint), display_text);
}

TEST_F(OmniboxAnswerActionTest, ConvertAction) {
  omnibox::RichAnswerTemplate answer_template;
  omnibox::SuggestionEnhancement* enhancement =
      answer_template.mutable_enhancements()->add_enhancements();

  scoped_refptr<OmniboxAction> upcasted_action =
      base::MakeRefCounted<OmniboxAnswerAction>(
          std::move(*enhancement), TemplateURLRef::SearchTermsArgs(),
          omnibox::ANSWER_TYPE_DICTIONARY);
  auto* downcasted_action =
      OmniboxAnswerAction::FromAction(upcasted_action.get());
  EXPECT_EQ(upcasted_action.get(), downcasted_action);
}

TEST_F(OmniboxAnswerActionTest, RecordMetrics) {
  omnibox::RichAnswerTemplate answer_template;
  omnibox::SuggestionEnhancement* enhancement =
      answer_template.mutable_enhancements()->add_enhancements();

  {
    auto action = base::MakeRefCounted<OmniboxAnswerAction>(
        std::move(*enhancement), TemplateURLRef::SearchTermsArgs(),
        omnibox::ANSWER_TYPE_DICTIONARY);
    base::HistogramTester histograms;
    action->RecordActionShown(1, /*executed = */ false);
    histograms.ExpectBucketCount("Omnibox.AnswerAction.Shown",
                                 omnibox::ANSWER_TYPE_DICTIONARY, 1);
    histograms.ExpectTotalCount("Omnibox.AnswerAction.Shown", 1);
    histograms.ExpectTotalCount("Omnibox.AnswerAction.Used", 0);
  }

  {
    auto action = base::MakeRefCounted<OmniboxAnswerAction>(
        std::move(*enhancement), TemplateURLRef::SearchTermsArgs(),
        omnibox::ANSWER_TYPE_FINANCE);
    base::HistogramTester histograms;
    action->RecordActionShown(1, /*executed = */ false);
    histograms.ExpectBucketCount("Omnibox.AnswerAction.Shown",
                                 omnibox::ANSWER_TYPE_FINANCE, 1);
    histograms.ExpectTotalCount("Omnibox.AnswerAction.Shown", 1);
    histograms.ExpectTotalCount("Omnibox.AnswerAction.Used", 0);
  }

  {
    auto action = base::MakeRefCounted<OmniboxAnswerAction>(
        std::move(*enhancement), TemplateURLRef::SearchTermsArgs(),
        omnibox::ANSWER_TYPE_FINANCE);
    base::HistogramTester histograms;
    action->RecordActionShown(1, /*executed = */ true);
    histograms.ExpectBucketCount("Omnibox.AnswerAction.Shown",
                                 omnibox::ANSWER_TYPE_FINANCE, 1);
    histograms.ExpectTotalCount("Omnibox.AnswerAction.Shown", 1);
    histograms.ExpectBucketCount("Omnibox.AnswerAction.Used",
                                 omnibox::ANSWER_TYPE_FINANCE, 1);
    histograms.ExpectTotalCount("Omnibox.AnswerAction.Used", 1);
  }
}
