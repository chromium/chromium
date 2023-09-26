// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_trigger_details.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

struct AddressRewriterInProfileSubsetMetricsTestCase {
  // Respective address lines of `profile_a` and `profile_b`
  std::u16string address_line_a;
  std::u16string address_line_b;
  // Expected histogram logged value.
  bool has_different_address;
};

class AddressRewriterInProfileSubsetMetricsTest
    : public autofill_metrics::AutofillMetricsBaseTest,
      public testing::TestWithParam<
          AddressRewriterInProfileSubsetMetricsTestCase> {
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

// Tests the logging of profile subsets having different street addresses, when
// the feature `kAutofillUseAddressRewriterInProfileSubsetComparison` is
// enabled.
TEST_P(AddressRewriterInProfileSubsetMetricsTest,
       AddressRewriterInProfileSubsetMetricsTestCase) {
  AddressRewriterInProfileSubsetMetricsTestCase test_case = GetParam();
  AutofillProfile profile_a;
  profile_a.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");
  profile_a.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, test_case.address_line_a);

  AutofillProfile profile_b;
  profile_b.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");
  profile_b.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, test_case.address_line_b);

  base::HistogramTester histogram_tester;
  const AutofillProfileComparator comparator("en-US");
  profile_a.IsSubsetOfForFieldSet(comparator, profile_b,
                                  {ADDRESS_HOME_STREET_ADDRESS});
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfilesDifferOnAddressLineOnly",
      test_case.has_different_address, 1);
}

INSTANTIATE_TEST_SUITE_P(AddressRewriterInProfileSubsetTest,
                         AddressRewriterInProfileSubsetMetricsTest,
                         testing::Values(
                             // Same street names up to rewriting.
                             AddressRewriterInProfileSubsetMetricsTestCase{
                                 .address_line_a = u"123 Main Street",
                                 .address_line_b = u"123 Main St",
                                 .has_different_address = false},
                             // Different street names even after rewriting.
                             AddressRewriterInProfileSubsetMetricsTestCase{
                                 .address_line_a = u"123 Main Street",
                                 .address_line_b = u"124 Main St",
                                 .has_different_address = true}));

TEST_F(AddressRewriterInProfileSubsetMetricsTest,
       UserAcceptsPreviouslyHiddenSuggestion) {
  AutofillProfile profile_a;
  profile_a.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");
  profile_a.SetRawInfo(NAME_FULL, u"first last");
  profile_a.SetRawInfo(ADDRESS_HOME_LINE1, u"123 Main Street");
  profile_a.SetRawInfo(EMAIL_ADDRESS, u"email@foo.com");
  personal_data().AddProfile(profile_a);

  AutofillProfile profile_b;
  profile_b.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");
  profile_b.SetRawInfo(NAME_FULL, u"first last");
  profile_b.SetRawInfo(ADDRESS_HOME_LINE1, u"124 Main Street");
  personal_data().AddProfile(profile_b);

  FormData form = test::CreateTestAddressFormData();

  base::UserActionTester user_action_tester;
  autofill_manager().OnFormsSeen({form}, {});
  external_delegate().OnQuery(form, form.fields.front(), gfx::RectF());
  external_delegate().DidAcceptSuggestion(
      test::CreateAutofillSuggestion(PopupItemId::kAddressEntry, u"first last",
                                     Suggestion::BackendId(profile_b.guid())),
      0, AutofillSuggestionTriggerSource::kUnspecified);

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Autofill_AcceptedPreviouslyHiddenSuggestion"));

  external_delegate().DidAcceptSuggestion(
      test::CreateAutofillSuggestion(PopupItemId::kAddressEntry, u"first last",
                                     Suggestion::BackendId(profile_a.guid())),
      0, AutofillSuggestionTriggerSource::kUnspecified);

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Autofill_AcceptedPreviouslyHiddenSuggestion"));
}

}  // namespace autofill::autofill_metrics
