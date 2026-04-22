// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/core/browser/metrics/plus_address_submission_logger.h"

#include <algorithm>
#include <string_view>
#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/integrators/password_form_classification.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
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

constexpr char kSamplePlusAddress[] = "plus@plus.com";

class PlusAddressSubmissionLoggerTest
    : public ::testing::Test,
      public autofill::WithTestAutofillClientDriverManager<
          autofill::TestAutofillClient,
          autofill::TestAutofillDriver,
          autofill::TestBrowserAutofillManager> {
 public:
  PlusAddressSubmissionLoggerTest()
      : submission_logger_(
            identity_manager(),
            base::BindRepeating(
                &PlusAddressSubmissionLoggerTest::VerifyPlusAddress,
                base::Unretained(this))) {
    SetPlusAddresses({kSamplePlusAddress});
  }

  void SetUp() override {
    InitAutofillClient();
    CreateAutofillDriver();
  }

  void TearDown() override { DestroyAutofillClient(); }

 protected:
  FormData GetEmailForm() {
    const auto field_types = std::vector<FieldType>({FieldType::EMAIL_ADDRESS});
    FormData form = autofill::test::GetFormData(field_types);
    autofill_manager().AddSeenForm(form, field_types);
    return form;
  }

  FormData GetLargeForm() {
    auto field_types = std::vector<FieldType>(39);
    field_types[0] = FieldType::EMAIL_ADDRESS;
    FormData form = autofill::test::GetFormData(field_types);
    autofill_manager().AddSeenForm(form, field_types);
    return form;
  }

  void SetPlusAddresses(std::vector<std::string> plus_addresses) {
    plus_addresses_ = std::move(plus_addresses);
  }

  std::vector<ukm::TestUkmRecorder::HumanReadableUkmMetrics> GetUkmMetrics() {
    return autofill_client().GetUkmRecorder()->GetMetrics(
        ukm::builders::PlusAddresses_Submission::kEntryName,
        {"FieldCountBrowserForm", "FieldCountRendererForm", "PlusAddressCount",
         "CheckoutOrCartPage", "ManagedProfile", "NewlyCreatedPlusAddress",
         "SubmittedPlusAddress", "PasswordFormType",
         "WasShownCreateSuggestion"});
  }

  bool VerifyPlusAddress(const std::string& plus_address) {
    return std::ranges::contains(plus_addresses_, plus_address);
  }

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
  PlusAddressSubmissionLogger submission_logger_;

  // The known set of plus addresses. Used for verifying whether a field's value
  // is a plus address.
  std::vector<std::string> plus_addresses_;
};

}  // namespace
}  // namespace plus_addresses::metrics
