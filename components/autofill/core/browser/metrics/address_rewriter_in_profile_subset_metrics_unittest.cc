// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_suggestion_generator.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_trigger_details.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

namespace {

class AddressRewriterInProfileSubsetMetricsTest
    : public autofill_metrics::AutofillMetricsBaseTest,
      public testing::Test {
 public:
  void SetUp() override {
    SetUpHelper();
    scoped_feature_list_.InitAndEnableFeature(
        features::kAutofillUseAddressRewriterInProfileSubsetComparison);
  }
  void TearDown() override { TearDownHelper(); }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that previously hidden suggestions are correctly set and detected and
// that we correctly emit the metrics responsible for counting those suggestions
// and detecting when the user accepts one of those suggestions.
TEST_F(AddressRewriterInProfileSubsetMetricsTest, PreviouslyHiddenSuggestion) {
  personal_data().ClearProfiles();

  AutofillProfile profile_a(AddressCountryCode("US"));
  profile_a.SetRawInfo(NAME_FULL, u"first last");
  profile_a.SetRawInfo(ADDRESS_HOME_LINE1, u"123 Main Street");
  profile_a.SetRawInfo(EMAIL_ADDRESS, u"email@foo.com");
  profile_a.set_use_count(100);
  personal_data().AddProfile(profile_a);

  AutofillProfile profile_b(AddressCountryCode("US"));
  profile_b.SetRawInfo(NAME_FULL, u"first last");
  profile_b.SetRawInfo(ADDRESS_HOME_LINE1, u"124 Main Street");
  personal_data().AddProfile(profile_b);

  FormData form = test::CreateTestAddressFormData();
  autofill_manager().OnFormsSeen({form}, {});
  external_delegate().OnQuery(form, form.fields.front(), gfx::RectF());

  base::HistogramTester histogram_tester;
  AutofillSuggestionGenerator suggestion_generator(autofill_client_.get(),
                                                   &personal_data());
  std::vector<Suggestion> suggestions =
      suggestion_generator.GetSuggestionsForProfiles(
          {NAME_FULL, ADDRESS_HOME_LINE1}, FormFieldData(), NAME_FULL,
          std::nullopt, AutofillSuggestionTriggerSource::kUnspecified);
  histogram_tester.ExpectUniqueSample(
      "Autofill.PreviouslyHiddenSuggestionNumber", 1, 1);
  ASSERT_EQ(suggestions.size(), 2u);
  EXPECT_FALSE(suggestions[0].hidden_prior_to_address_rewriter_usage);
  EXPECT_TRUE(suggestions[1].hidden_prior_to_address_rewriter_usage);

  external_delegate().DidAcceptSuggestion(
      suggestions[0], AutofillPopupDelegate::SuggestionPosition{.row = 0},
      AutofillSuggestionTriggerSource::kUnspecified);
  histogram_tester.ExpectUniqueSample(
      "Autofill.AcceptedPreviouslyHiddenSuggestion", 0, 1);

  external_delegate().DidAcceptSuggestion(
      suggestions[1], AutofillPopupDelegate::SuggestionPosition{.row = 1},
      AutofillSuggestionTriggerSource::kUnspecified);
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.AcceptedPreviouslyHiddenSuggestion"),
              base::BucketsAre(base::Bucket(0, 1), base::Bucket(1, 1)));
}

}  // namespace

}  // namespace autofill::autofill_metrics
