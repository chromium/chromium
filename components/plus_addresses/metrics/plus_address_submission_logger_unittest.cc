// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/metrics/plus_address_submission_logger.h"

#include <string_view>
#include <utility>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/autofill_plus_address_delegate.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/password_form_classification.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/ukm/test_ukm_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace plus_addresses::metrics {
namespace {

using ::autofill::FieldType;
using ::autofill::FormData;
using ::autofill::SuggestionType;
using ::autofill::test::FormDescription;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using PasswordFormType = autofill::PasswordFormClassification::Type;
using SuggestionContext =
    autofill::AutofillPlusAddressDelegate::SuggestionContext;

constexpr char kGaiaAccount[] = "foo123@gmail.com";
constexpr char16_t kGaiaAccount_U16[] = u"foo123@gmail.com";
constexpr char kSamplePlusAddress[] = "plus@plus.com";
constexpr char16_t kSamplePlusAddress_U16[] = u"plus@plus.com";
constexpr auto kSubmissionSource =
    autofill::mojom::SubmissionSource::FORM_SUBMISSION;

constexpr char kNonCommerceUrl[] = "https://www.foo.com";
constexpr char kCommerceUrl[] = "https://www.buy-stuff.com/checkout.html";
constexpr char kManagedDomain[] = "corporate.com";

// Short-hands for the bucket enum used to record bucketed plus address counts.
constexpr int64_t kNoPlusAddress = 0;
constexpr int64_t kOneToThreePlusAddresses = 1;
constexpr int64_t kMoreThanThreePlusAddresses = 2;

ukm::TestUkmRecorder::HumanReadableUkmMetrics CreateUkmMetrics(
    size_t field_count_browser_form,
    size_t field_count_renderer_form,
    int64_t plus_address_count,
    bool is_checkout_or_cart_page,
    bool is_managed,
    bool is_newly_created,
    bool submitted_plus_address,
    PasswordFormType password_form_type,
    SuggestionContext suggestion_context,
    bool was_shown_create_suggestion) {
  ukm::TestUkmRecorder::HumanReadableUkmMetrics metrics;
  metrics["FieldCountBrowserForm"] = field_count_browser_form;
  metrics["FieldCountRendererForm"] = field_count_renderer_form;
  metrics["PlusAddressCount"] = plus_address_count;
  metrics["CheckoutOrCartPage"] = is_checkout_or_cart_page;
  metrics["ManagedProfile"] = is_managed;
  metrics["NewlyCreatedPlusAddress"] = is_newly_created;
  metrics["SubmittedPlusAddress"] = submitted_plus_address;
  metrics["PasswordFormType"] = base::to_underlying(password_form_type);
  metrics["SuggestionContext"] = base::to_underlying(suggestion_context);
  metrics["WasShownCreateSuggestion"] = was_shown_create_suggestion;
  return metrics;
}

class PlusAddressSubmissionLoggerTest : public ::testing::Test {
 public:
  PlusAddressSubmissionLoggerTest()
      : submission_logger_(
            identity_manager(),
            base::BindRepeating(
                &PlusAddressSubmissionLoggerTest::VerifyPlusAddress,
                base::Unretained(this))) {
    SetPlusAddresses({kSamplePlusAddress});
  }

 protected:
  FormData GetEmailForm() {
    const auto field_types = std::vector<FieldType>({FieldType::EMAIL_ADDRESS});
    FormData form = autofill::test::GetFormData(field_types);
    autofill_manager_.AddSeenForm(form, field_types);
    return form;
  }

  FormData GetLargeForm() {
    auto field_types = std::vector<FieldType>(39);
    field_types[0] = FieldType::EMAIL_ADDRESS;
    FormData form = autofill::test::GetFormData(field_types);
    autofill_manager_.AddSeenForm(form, field_types);
    return form;
  }

  void SetPlusAddresses(std::vector<std::string> plus_addresses) {
    plus_addresses_ = std::move(plus_addresses);
  }

  std::vector<ukm::TestUkmRecorder::HumanReadableUkmMetrics> GetUkmMetrics() {
    return autofill_client_.GetTestUkmRecorder()->GetMetrics(
        ukm::builders::PlusAddresses_Submission::kEntryName,
        {"FieldCountBrowserForm", "FieldCountRendererForm", "PlusAddressCount",
         "CheckoutOrCartPage", "ManagedProfile", "NewlyCreatedPlusAddress",
         "SubmittedPlusAddress", "PasswordFormType", "SuggestionContext",
         "WasShownCreateSuggestion"});
  }

  bool VerifyPlusAddress(const std::string& plus_address) {
    return base::Contains(plus_addresses_, plus_address);
  }

  autofill::TestBrowserAutofillManager& autofill_manager() {
    return autofill_manager_;
  }
  autofill::TestAutofillClient& autofill_client() { return autofill_client_; }
  signin::IdentityTestEnvironment& identity_env() { return identity_test_env_; }
  signin::IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }
  PlusAddressSubmissionLogger& submission_logger() {
    return submission_logger_;
  }

 private:
  base::test::ScopedFeatureList feature_list_{};
  base::test::TaskEnvironment task_environment_;
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  autofill::TestAutofillClient autofill_client_;
  autofill::TestAutofillDriver autofill_driver_{&autofill_client_};
  autofill::TestBrowserAutofillManager autofill_manager_{&autofill_driver_};
  PlusAddressSubmissionLogger submission_logger_;

  // The known set of plus addresses. Used for verifying whether a field's value
  // is a plus address.
  std::vector<std::string> plus_addresses_;
};

// Tests that no metrics are recorded for signed out users.
TEST_F(PlusAddressSubmissionLoggerTest, NoMetricForSignedOutUsers) {
  FormData form = GetEmailForm();
  submission_logger().OnPlusAddressSuggestionShown(
      autofill_manager(), form.global_id(), test_api(form).field(0).global_id(),
      SuggestionContext::kAutofillProfileOnEmailField,
      PasswordFormType::kNoPasswordForm,
      SuggestionType::kFillExistingPlusAddress,
      /*plus_address_count=*/1);

  test_api(form).field(0).set_value(kSamplePlusAddress_U16);
  autofill_manager().OnFormSubmitted(form, /*known_success=*/true,
                                     kSubmissionSource);
  EXPECT_THAT(GetUkmMetrics(), IsEmpty());
}

struct PlusAddressSubmissionTestCase {
  struct Input {
    enum class SampleForm {
      // A form with a single email field.
      kEmailForm,
      // A form with 39 fields, of which the first one has an email
      // classification.
      kLargeForm
    };
    SampleForm sample_form = SampleForm::kEmailForm;
    SuggestionContext context = SuggestionContext::kAutofillProfileOnEmailField;
    PasswordFormType form_type = PasswordFormType::kNoPasswordForm;
    SuggestionType suggestion_type = SuggestionType::kCreateNewPlusAddress;
    int64_t plus_address_count = kNoPlusAddress;
    std::u16string submitted_value;
    bool is_managed_profile = false;
    std::string main_frame_url = kNonCommerceUrl;
  };
  const Input input;

  const std::vector<ukm::TestUkmRecorder::HumanReadableUkmMetrics> ukms;
  struct Uma {
    // If set, contains the value of the single `PlusAddresses.Submission` that
    // was emitted. Otherwise, no `PlusAddresses.Submission` emission is
    // expected.
    std::optional<bool> submitted_plus_address;
    // If set, contains the value of the single
    // `PlusAddress.Submission.FirstTimeUser.No` histogram that was emitted.
    // Otherwise, no emission is expected for this histogram.
    std::optional<bool> submitted_plus_address_first_time_user_no;
    // If set, contains the value of the single
    // `PlusAddress.Submission.FirstTimeUser.Yes` histogram that was emitted.
    // Otherwise, no emission is expected for this histogram.
    std::optional<bool> submitted_plus_address_first_time_user_yes;
    // If set, contains the value of the single
    // `PlusAddress.Submission.ManagedUser.No` histogram that was emitted.
    // Otherwise, no emission is expected for this histogram.
    std::optional<bool> submitted_plus_address_managed_user_no;
    // If set, contains the value of the single
    // `PlusAddress.Submission.ManagedUser.Yes` histogram that was emitted.
    // Otherwise, no emission is expected for this histogram.
    std::optional<bool> submitted_plus_address_managed_user_yes;
    // If set, contains the value of the
    // `PlusAddress.Submission.IsSingleFieldRendererForm' histogram that was
    // emitted. Otherwise, no emission is expected for this histogram.
    std::optional<bool> submitted_plus_address_is_single_field_renderer_form;
    // If set, contains the value of the
    // `PlusAddress.Submission.IsSingleFieldRendererForm.ManagedUser.No'
    // histogram that was emitted. Otherwise, no emission is expected for this
    // histogram.
    std::optional<bool>
        submitted_plus_address_is_single_field_renderer_form_managed_user_no;
  };
  const Uma uma;
};

class PlusAddressSubmissionTestWithParam
    : public PlusAddressSubmissionLoggerTest,
      public ::testing::WithParamInterface<PlusAddressSubmissionTestCase> {};

// Parametrized test that checks that the expected UKM is recorded. The test
// simulates that the user is logged in and submits a previously focused form
TEST_P(PlusAddressSubmissionTestWithParam, SubmittingFormRecordsUkm) {
  base::HistogramTester histogram_tester;
  const PlusAddressSubmissionTestCase::Input& input = GetParam().input;

  AccountInfo account_info = identity_env().MakeAccountAvailable(
      kGaiaAccount, {signin::ConsentLevel::kSignin});
  if (input.is_managed_profile) {
    account_info.hosted_domain = kManagedDomain;
    identity_env().UpdateAccountInfoForAccount(account_info);
  }
  autofill_client().set_last_committed_primary_main_frame_url(
      GURL(input.main_frame_url));

  FormData form = [&] {
    switch (input.sample_form) {
      using enum PlusAddressSubmissionTestCase::Input::SampleForm;
      case kEmailForm:
        return GetEmailForm();
      case kLargeForm:
        return GetLargeForm();
    }
  }();
  submission_logger().OnPlusAddressSuggestionShown(
      autofill_manager(), form.global_id(), test_api(form).field(0).global_id(),
      input.context, input.form_type, input.suggestion_type,
      input.plus_address_count);

  test_api(form).field(0).set_value(input.submitted_value);
  autofill_manager().OnFormSubmitted(form, /*known_success=*/true,
                                     kSubmissionSource);
  EXPECT_THAT(GetUkmMetrics(), ElementsAreArray(GetParam().ukms));

  auto check_boolean_histogram = [&](std::string_view histogram_suffix,
                                     std::optional<bool> expected_value) {
    const std::string histogram = base::StrCat(
        {PlusAddressSubmissionLogger::kUmaSubmissionPrefix, histogram_suffix});
    if (expected_value) {
      histogram_tester.ExpectUniqueSample(histogram, *expected_value, 1);
    } else {
      histogram_tester.ExpectTotalCount(histogram, 0);
    }
  };
  const PlusAddressSubmissionTestCase::Uma& uma = GetParam().uma;
  check_boolean_histogram("", uma.submitted_plus_address);
  check_boolean_histogram(".ManagedUser.No",
                          uma.submitted_plus_address_managed_user_no);
  check_boolean_histogram(".ManagedUser.Yes",
                          uma.submitted_plus_address_managed_user_yes);
  check_boolean_histogram(
      ".SingleFieldRendererForm",
      uma.submitted_plus_address_is_single_field_renderer_form);
  check_boolean_histogram(
      ".SingleFieldRendererForm.ManagedUser.No",
      uma.submitted_plus_address_is_single_field_renderer_form_managed_user_no);
}

INSTANTIATE_TEST_SUITE_P(
    PlusAddressSubmissionTest,
    PlusAddressSubmissionTestWithParam,
    ::testing::Values(
        PlusAddressSubmissionTestCase{
            // Submission of an email form after creating and filling a new plus
            // address.
            .input = {.context =
                          SuggestionContext::kAutofillProfileOnEmailField,
                      .form_type = PasswordFormType::kNoPasswordForm,
                      .suggestion_type = SuggestionType::kCreateNewPlusAddress,
                      .plus_address_count = 0,
                      .submitted_value = kSamplePlusAddress_U16},
            .ukms = {CreateUkmMetrics(
                /*field_count_browser_form=*/1,
                /*field_count_renderer_form=*/1,
                /*plus_address_count=*/kNoPlusAddress,
                /*is_checkout_or_cart_page=*/false,
                /*is_managed=*/false,
                /*is_newly_created=*/true,
                /*submitted_plus_address=*/true,
                PasswordFormType::kNoPasswordForm,
                SuggestionContext::kAutofillProfileOnEmailField,
                /*was_shown_create_suggestion=*/true)},
            .uma =
                {.submitted_plus_address = true,
                 .submitted_plus_address_first_time_user_yes = true,
                 .submitted_plus_address_managed_user_no = true,
                 .submitted_plus_address_is_single_field_renderer_form = true,
                 .submitted_plus_address_is_single_field_renderer_form_managed_user_no =
                     true}},
        // Submission of an email form after seeing combined plus address &
        // Autocomplete suggestions and creating and filling a new plus address.
        PlusAddressSubmissionTestCase{
            .input = {.context = SuggestionContext::kAutocomplete,
                      .form_type = PasswordFormType::kNoPasswordForm,
                      .suggestion_type = SuggestionType::kCreateNewPlusAddress,
                      .plus_address_count = 0,
                      .submitted_value = kSamplePlusAddress_U16},
            .ukms = {CreateUkmMetrics(
                /*field_count_browser_form=*/1,
                /*field_count_renderer_form=*/1,
                /*plus_address_count=*/kNoPlusAddress,
                /*is_checkout_or_cart_page=*/false,
                /*is_managed=*/false,
                /*is_newly_created=*/true,
                /*submitted_plus_address=*/true,
                PasswordFormType::kNoPasswordForm,
                SuggestionContext::kAutocomplete,
                /*was_shown_create_suggestion=*/true)},
            .uma =
                {.submitted_plus_address = true,
                 .submitted_plus_address_first_time_user_yes = true,
                 .submitted_plus_address_managed_user_no = true,
                 .submitted_plus_address_is_single_field_renderer_form = true,
                 .submitted_plus_address_is_single_field_renderer_form_managed_user_no =
                     true}},
        // Submission of an email form after filling an existing plus address.
        PlusAddressSubmissionTestCase{
            .input =
                {.context = SuggestionContext::kAutofillProfileOnEmailField,
                 .form_type = PasswordFormType::kSingleUsernameForm,
                 .suggestion_type = SuggestionType::kFillExistingPlusAddress,
                 .plus_address_count = 1,
                 .submitted_value = kSamplePlusAddress_U16},
            .ukms = {CreateUkmMetrics(
                /*field_count_browser_form=*/1,
                /*field_count_renderer_form=*/1,
                /*plus_address_count=*/kOneToThreePlusAddresses,
                /*is_checkout_or_cart_page=*/false,
                /*is_managed=*/false,
                /*is_newly_created=*/false,
                /*submitted_plus_address=*/true,
                PasswordFormType::kSingleUsernameForm,
                SuggestionContext::kAutofillProfileOnEmailField,
                /*was_shown_create_suggestion=*/false)},
            .uma =
                {.submitted_plus_address = true,
                 .submitted_plus_address_first_time_user_no = true,
                 .submitted_plus_address_managed_user_no = true,
                 .submitted_plus_address_is_single_field_renderer_form = true,
                 .submitted_plus_address_is_single_field_renderer_form_managed_user_no =
                     true}},
        // Submission from a managed account.
        PlusAddressSubmissionTestCase{
            .input =
                {.context = SuggestionContext::kAutofillProfileOnEmailField,
                 .form_type = PasswordFormType::kSingleUsernameForm,
                 .suggestion_type = SuggestionType::kFillExistingPlusAddress,
                 .plus_address_count = 1,
                 .submitted_value = kSamplePlusAddress_U16,
                 .is_managed_profile = true},
            .ukms = {CreateUkmMetrics(
                /*field_count_browser_form=*/1,
                /*field_count_renderer_form=*/1,
                /*plus_address_count=*/kOneToThreePlusAddresses,
                /*is_checkout_or_cart_page=*/false,
                /*is_managed=*/true,
                /*is_newly_created=*/false,
                /*submitted_plus_address=*/true,
                PasswordFormType::kSingleUsernameForm,
                SuggestionContext::kAutofillProfileOnEmailField,
                /*was_shown_create_suggestion=*/false)},
            .uma = {.submitted_plus_address = true,
                    .submitted_plus_address_first_time_user_no = true,
                    .submitted_plus_address_managed_user_yes = true,
                    .submitted_plus_address_is_single_field_renderer_form =
                        true}},
        // Submission from a main frame URL with a checkout context.
        PlusAddressSubmissionTestCase{
            .input =
                {.context = SuggestionContext::kAutofillProfileOnEmailField,
                 .form_type = PasswordFormType::kSingleUsernameForm,
                 .suggestion_type = SuggestionType::kFillExistingPlusAddress,
                 .plus_address_count = 1,
                 .submitted_value = kSamplePlusAddress_U16,
                 .main_frame_url = kCommerceUrl},
            .ukms = {CreateUkmMetrics(
                /*field_count_browser_form=*/1,
                /*field_count_renderer_form=*/1,
                /*plus_address_count=*/kOneToThreePlusAddresses,
                /*is_checkout_or_cart_page=*/true,
                /*is_managed=*/false,
                /*is_newly_created=*/false,
                /*submitted_plus_address=*/true,
                PasswordFormType::kSingleUsernameForm,
                SuggestionContext::kAutofillProfileOnEmailField,
                /*was_shown_create_suggestion=*/false)},
            .uma =
                {.submitted_plus_address = true,
                 .submitted_plus_address_first_time_user_no = true,
                 .submitted_plus_address_managed_user_no = true,
                 .submitted_plus_address_is_single_field_renderer_form = true,
                 .submitted_plus_address_is_single_field_renderer_form_managed_user_no =
                     true}},
        // Submission of an email form with GAIA email after seeing a fill plus
        // address suggestion.
        PlusAddressSubmissionTestCase{
            .input =
                {.context = SuggestionContext::kAutofillProfileOnEmailField,
                 .form_type = PasswordFormType::kSingleUsernameForm,
                 .suggestion_type = SuggestionType::kFillExistingPlusAddress,
                 .plus_address_count = 4,
                 .submitted_value = kGaiaAccount_U16},
            .ukms = {CreateUkmMetrics(
                /*field_count_browser_form=*/1,
                /*field_count_renderer_form=*/1,
                /*plus_address_count=*/kMoreThanThreePlusAddresses,
                /*is_checkout_or_cart_page=*/false,
                /*is_managed=*/false,
                /*is_newly_created=*/false,
                /*submitted_plus_address=*/false,
                PasswordFormType::kSingleUsernameForm,
                SuggestionContext::kAutofillProfileOnEmailField,
                /*was_shown_create_suggestion=*/false)},
            .uma =
                {.submitted_plus_address = false,
                 .submitted_plus_address_first_time_user_no = false,
                 .submitted_plus_address_managed_user_no = false,
                 .submitted_plus_address_is_single_field_renderer_form = false,
                 .submitted_plus_address_is_single_field_renderer_form_managed_user_no =
                     false}},
        // Submission of an email form with GAIA email after seeing a create
        // plus address suggestion.
        PlusAddressSubmissionTestCase{
            .input = {.context =
                          SuggestionContext::kAutofillProfileOnEmailField,
                      .form_type = PasswordFormType::kNoPasswordForm,
                      .suggestion_type = SuggestionType::kCreateNewPlusAddress,
                      .plus_address_count = 1,
                      .submitted_value = kGaiaAccount_U16},
            .ukms = {CreateUkmMetrics(
                /*field_count_browser_form=*/1,
                /*field_count_renderer_form=*/1,
                /*plus_address_count=*/kOneToThreePlusAddresses,
                /*is_checkout_or_cart_page=*/false,
                /*is_managed=*/false,
                /*is_newly_created=*/false,
                /*submitted_plus_address=*/false,
                PasswordFormType::kNoPasswordForm,
                SuggestionContext::kAutofillProfileOnEmailField,
                /*was_shown_create_suggestion=*/true)},
            .uma =
                {.submitted_plus_address = false,
                 .submitted_plus_address_first_time_user_no = false,
                 .submitted_plus_address_managed_user_no = false,
                 .submitted_plus_address_is_single_field_renderer_form = false,
                 .submitted_plus_address_is_single_field_renderer_form_managed_user_no =
                     false}},
        // Submission of a form with many fields - the field counts are
        // bucketed.
        PlusAddressSubmissionTestCase{
            .input = {.sample_form = PlusAddressSubmissionTestCase::Input::
                          SampleForm::kLargeForm,
                      .context =
                          SuggestionContext::kAutofillProfileOnEmailField,
                      .form_type = PasswordFormType::kNoPasswordForm,
                      .suggestion_type = SuggestionType::kCreateNewPlusAddress,
                      .plus_address_count = 1,
                      .submitted_value = kSamplePlusAddress_U16},
            .ukms = {CreateUkmMetrics(
                /*field_count_browser_form=*/38,
                /*field_count_renderer_form=*/38,
                /*plus_address_count=*/kOneToThreePlusAddresses,
                /*is_checkout_or_cart_page=*/false,
                /*is_managed=*/false,
                /*is_newly_created=*/true,
                /*submitted_plus_address=*/true,
                PasswordFormType::kNoPasswordForm,
                SuggestionContext::kAutofillProfileOnEmailField,
                /*was_shown_create_suggestion=*/true)},
            .uma = {.submitted_plus_address = true,
                    .submitted_plus_address_first_time_user_no = true,
                    .submitted_plus_address_managed_user_no = true}},
        // Submission of an email form after filling no email address at all.
        PlusAddressSubmissionTestCase{
            .input =
                {.context = SuggestionContext::kAutofillProfileOnEmailField,
                 .form_type = PasswordFormType::kSingleUsernameForm,
                 .suggestion_type = SuggestionType::kFillExistingPlusAddress,
                 .plus_address_count = kOneToThreePlusAddresses,
                 .submitted_value = u"Some name"},
            .ukms = {},
            .uma = {}}));

}  // namespace
}  // namespace plus_addresses::metrics
