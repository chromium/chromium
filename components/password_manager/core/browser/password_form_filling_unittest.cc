// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form_filling.h"

#include <map>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using autofill::PasswordForm;
using autofill::PasswordFormFillData;
using base::ASCIIToUTF16;
using testing::_;
using testing::Return;
using testing::SaveArg;

namespace password_manager {
namespace {
class MockPasswordManagerDriver : public StubPasswordManagerDriver {
 public:
  MockPasswordManagerDriver() {}

  ~MockPasswordManagerDriver() override = default;

  MOCK_CONST_METHOD0(GetId, int());
  MOCK_METHOD1(FillPasswordForm, void(const PasswordFormFillData&));
  MOCK_METHOD0(InformNoSavedCredentials, void());
  MOCK_METHOD1(ShowInitialPasswordAccountSuggestions,
               void(const PasswordFormFillData&));
  MOCK_METHOD1(AllowPasswordGenerationForForm, void(const PasswordForm&));
};

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MOCK_METHOD3(PasswordWasAutofilled,
               void(const std::vector<const PasswordForm*>&,
                    const GURL&,
                    const std::vector<const PasswordForm*>*));

  MOCK_CONST_METHOD0(IsMainFrameSecure, bool());
};

}  // namespace

class PasswordFormFillingTest : public testing::Test {
 public:
  PasswordFormFillingTest() {
    ON_CALL(client_, IsMainFrameSecure()).WillByDefault(Return(true));

    observed_form_.origin = GURL("https://accounts.google.com/a/LoginAuth");
    observed_form_.action = GURL("https://accounts.google.com/a/Login");
    observed_form_.username_element = ASCIIToUTF16("Email");
    observed_form_.username_element_renderer_id = 100;
    observed_form_.password_element = ASCIIToUTF16("Passwd");
    observed_form_.password_element_renderer_id = 101;
    observed_form_.submit_element = ASCIIToUTF16("signIn");
    observed_form_.signon_realm = "https://accounts.google.com";
    observed_form_.form_data.name = ASCIIToUTF16("the-form-name");

    saved_match_ = observed_form_;
    saved_match_.origin =
        GURL("https://accounts.google.com/a/ServiceLoginAuth");
    saved_match_.action = GURL("https://accounts.google.com/a/ServiceLogin");
    saved_match_.preferred = true;
    saved_match_.username_value = ASCIIToUTF16("test@gmail.com");
    saved_match_.password_value = ASCIIToUTF16("test1");

    psl_saved_match_ = saved_match_;
    psl_saved_match_.is_public_suffix_match = true;
    psl_saved_match_.origin =
        GURL("https://m.accounts.google.com/a/ServiceLoginAuth");
    psl_saved_match_.action = GURL("https://m.accounts.google.com/a/Login");
    psl_saved_match_.signon_realm = "https://m.accounts.google.com";

    metrics_recorder_ = base::MakeRefCounted<PasswordFormMetricsRecorder>(
        true, client_.GetUkmSourceId());
  }

 protected:
  MockPasswordManagerDriver driver_;
  MockPasswordManagerClient client_;
  PasswordForm observed_form_;
  PasswordForm saved_match_;
  PasswordForm psl_saved_match_;
  scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder_;
  std::vector<const PasswordForm*> federated_matches_;
};

TEST_F(PasswordFormFillingTest, NoSavedCredentials) {
  std::vector<const PasswordForm*> best_matches;

  EXPECT_CALL(driver_, InformNoSavedCredentials());
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(0);
  EXPECT_CALL(driver_, ShowInitialPasswordAccountSuggestions(_)).Times(0);

  LikelyFormFilling likely_form_filling = SendFillInformationToRenderer(
      &client_, &driver_, observed_form_, best_matches, federated_matches_,
      nullptr, metrics_recorder_.get());
  EXPECT_EQ(LikelyFormFilling::kNoFilling, likely_form_filling);
}

TEST_F(PasswordFormFillingTest, Autofill) {
  std::vector<const PasswordForm*> best_matches;
  best_matches.push_back(&saved_match_);
  PasswordForm another_saved_match = saved_match_;
  another_saved_match.username_value += ASCIIToUTF16("1");
  another_saved_match.password_value += ASCIIToUTF16("1");
  best_matches.push_back(&another_saved_match);

  EXPECT_CALL(driver_, InformNoSavedCredentials()).Times(0);
  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, FillPasswordForm(_)).WillOnce(SaveArg<0>(&fill_data));
  EXPECT_CALL(driver_, ShowInitialPasswordAccountSuggestions(_)).Times(0);
  EXPECT_CALL(client_, PasswordWasAutofilled(_, _, _));

  LikelyFormFilling likely_form_filling = SendFillInformationToRenderer(
      &client_, &driver_, observed_form_, best_matches, federated_matches_,
      &saved_match_, metrics_recorder_.get());
  EXPECT_EQ(LikelyFormFilling::kFillOnPageLoad, likely_form_filling);

  // Check that the message to the renderer (i.e. |fill_data|) is filled
  // correctly.
  EXPECT_EQ(observed_form_.origin, fill_data.origin);
  EXPECT_FALSE(fill_data.wait_for_username);
  EXPECT_EQ(observed_form_.username_element, fill_data.username_field.name);
  EXPECT_EQ(saved_match_.username_value, fill_data.username_field.value);
  EXPECT_EQ(observed_form_.password_element, fill_data.password_field.name);
  EXPECT_EQ(saved_match_.password_value, fill_data.password_field.value);

  // Check that information about non-preferred best matches is filled.
  ASSERT_EQ(1u, fill_data.additional_logins.size());
  EXPECT_EQ(another_saved_match.username_value,
            fill_data.additional_logins.begin()->first);
  EXPECT_EQ(another_saved_match.password_value,
            fill_data.additional_logins.begin()->second.password);
  // Realm is empty for non-psl match.
  EXPECT_TRUE(fill_data.additional_logins.begin()->second.realm.empty());
}

TEST_F(PasswordFormFillingTest, TestFillOnLoadSuggestion) {
  const struct {
    const char* description;
    bool new_password_present;
    bool current_password_present;
  } kTestCases[] = {
      {
          .description = "No new, some current",
          .new_password_present = false,
          .current_password_present = true,
      },
      {
          .description = "No current, some new",
          .new_password_present = true,
          .current_password_present = false,
      },
      {
          .description = "Both",
          .new_password_present = true,
          .current_password_present = true,
      },
  };
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);
    std::vector<const PasswordForm*> best_matches = {&saved_match_};

    PasswordForm observed_form = observed_form_;
    if (test_case.new_password_present) {
      observed_form.new_password_element = ASCIIToUTF16("New Passwd");
      observed_form.new_password_element_renderer_id = 125;
    }
    if (!test_case.current_password_present) {
      observed_form.password_element.clear();
      observed_form.password_element_renderer_id =
          autofill::FormFieldData::kNotSetFormControlRendererId;
    }

    PasswordFormFillData fill_data;
    EXPECT_CALL(driver_, FillPasswordForm(_)).WillOnce(SaveArg<0>(&fill_data));
    EXPECT_CALL(client_, PasswordWasAutofilled(_, _, _));

    LikelyFormFilling likely_form_filling = SendFillInformationToRenderer(
        &client_, &driver_, observed_form, best_matches, federated_matches_,
        &saved_match_, metrics_recorder_.get());

    // In all cases where a current password exists, fill on load should be
    // permitted. Otherwise, the renderer will not fill anyway and return
    // kFillOnAccountSelect.
    if (test_case.current_password_present)
      EXPECT_EQ(LikelyFormFilling::kFillOnPageLoad, likely_form_filling);
    else
      EXPECT_EQ(LikelyFormFilling::kFillOnAccountSelect, likely_form_filling);
  }
}

TEST_F(PasswordFormFillingTest, AutofillPSLMatch) {
  std::vector<const PasswordForm*> best_matches = {&psl_saved_match_};

  EXPECT_CALL(driver_, InformNoSavedCredentials()).Times(0);
  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, FillPasswordForm(_)).WillOnce(SaveArg<0>(&fill_data));
  EXPECT_CALL(driver_, ShowInitialPasswordAccountSuggestions(_)).Times(0);
  EXPECT_CALL(client_, PasswordWasAutofilled(_, _, _));

  LikelyFormFilling likely_form_filling = SendFillInformationToRenderer(
      &client_, &driver_, observed_form_, best_matches, federated_matches_,
      &psl_saved_match_, metrics_recorder_.get());
  EXPECT_EQ(LikelyFormFilling::kFillOnAccountSelect, likely_form_filling);

  // Check that the message to the renderer (i.e. |fill_data|) is filled
  // correctly.
  EXPECT_EQ(observed_form_.origin, fill_data.origin);
  EXPECT_TRUE(fill_data.wait_for_username);
  EXPECT_EQ(psl_saved_match_.signon_realm, fill_data.preferred_realm);
  EXPECT_EQ(observed_form_.username_element, fill_data.username_field.name);
  EXPECT_EQ(saved_match_.username_value, fill_data.username_field.value);
  EXPECT_EQ(observed_form_.password_element, fill_data.password_field.name);
  EXPECT_EQ(saved_match_.password_value, fill_data.password_field.value);
}

TEST_F(PasswordFormFillingTest, NoAutofillOnHttp) {
  PasswordForm observed_http_form = observed_form_;
  observed_http_form.origin = GURL("http://accounts.google.com/a/LoginAuth");
  observed_http_form.action = GURL("http://accounts.google.com/a/Login");
  observed_http_form.signon_realm = "http://accounts.google.com";

  PasswordForm saved_http_match = saved_match_;
  saved_http_match.origin =
      GURL("http://accounts.google.com/a/ServiceLoginAuth");
  saved_http_match.action = GURL("http://accounts.google.com/a/ServiceLogin");
  saved_http_match.signon_realm = "http://accounts.google.com";

  ASSERT_FALSE(GURL(saved_http_match.signon_realm).SchemeIsCryptographic());
  std::vector<const PasswordForm*> best_matches = {&saved_http_match};

  EXPECT_CALL(client_, IsMainFrameSecure).WillOnce(Return(false));
  LikelyFormFilling likely_form_filling = SendFillInformationToRenderer(
      &client_, &driver_, observed_http_form, best_matches, federated_matches_,
      &saved_http_match, metrics_recorder_.get());
  EXPECT_EQ(LikelyFormFilling::kFillOnAccountSelect, likely_form_filling);
}

#if defined(OS_ANDROID)
TEST_F(PasswordFormFillingTest, TouchToFill) {
  std::vector<const PasswordForm*> best_matches = {&saved_match_};

  for (bool enable_touch_to_fill : {false, true}) {
    SCOPED_TRACE(testing::Message() << "Enable Touch To Fill: "
                                    << std::boolalpha << enable_touch_to_fill);
    base::test::ScopedFeatureList features;
    features.InitWithFeatureState(autofill::features::kAutofillTouchToFill,
                                  enable_touch_to_fill);

    LikelyFormFilling likely_form_filling = SendFillInformationToRenderer(
        &client_, &driver_, observed_form_, best_matches, federated_matches_,
        &saved_match_, metrics_recorder_.get());
    EXPECT_EQ(enable_touch_to_fill ? LikelyFormFilling::kFillOnAccountSelect
                                   : LikelyFormFilling::kFillOnPageLoad,
              likely_form_filling);
  }
}
#endif

}  // namespace password_manager
