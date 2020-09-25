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
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/renderer_id.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using autofill::FieldRendererId;
using autofill::FormData;
using autofill::FormRendererId;
using autofill::PasswordFormFillData;
using base::ASCIIToUTF16;
using base::UTF16ToASCII;
using testing::_;
using testing::Return;
using testing::SaveArg;
using Store = password_manager::PasswordForm::Store;

namespace password_manager {
namespace {

constexpr char kPreferredUsername[] = "test@gmail.com";
constexpr char kPreferredPassword[] = "password";
constexpr char kPreferredAlternatePassword[] = "new_password";

constexpr char kDuplicateLocalUsername[] = "local@gmail.com";
constexpr char kDuplicateLocalPassword[] = "local_password";

constexpr char kSyncedUsername[] = "synced@gmail.com";
constexpr char kSyncedPassword[] = "password";

class MockPasswordManagerDriver : public StubPasswordManagerDriver {
 public:
  MOCK_METHOD(int, GetId, (), (const, override));
  MOCK_METHOD(void,
              FillPasswordForm,
              (const PasswordFormFillData&),
              (override));
  MOCK_METHOD(void, InformNoSavedCredentials, (bool), (override));
};

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MOCK_METHOD(void,
              PasswordWasAutofilled,
              (const std::vector<const PasswordForm*>&,
               const url::Origin&,
               const std::vector<const PasswordForm*>*),
              (override));
  MOCK_METHOD(bool, IsCommittedMainFrameSecure, (), (const, override));
};

PasswordForm CreateForm(std::string username,
                        std::string password,
                        Store store) {
  PasswordForm form;
  form.username_value = ASCIIToUTF16(username);
  form.password_value = ASCIIToUTF16(password);
  form.in_store = store;
  return form;
}

// Matcher for PasswordAndMetadata.
MATCHER_P3(IsLogin, username, password, uses_account_store, std::string()) {
  return UTF16ToASCII(arg.username) == username &&
         UTF16ToASCII(arg.password) == password &&
         arg.uses_account_store == uses_account_store;
}

PasswordFormFillData::LoginCollection::const_iterator FindPasswordByUsername(
    const std::vector<autofill::PasswordAndMetadata>& logins,
    const base::string16& username) {
  return std::find_if(logins.begin(), logins.end(),
                      [&username](const autofill::PasswordAndMetadata& login) {
                        return login.username == username;
                      });
}

}  // namespace

class PasswordFormFillingTest : public testing::Test {
 public:
  PasswordFormFillingTest() {
    ON_CALL(client_, IsCommittedMainFrameSecure()).WillByDefault(Return(true));

    observed_form_.url = GURL("https://accounts.google.com/a/LoginAuth");
    observed_form_.action = GURL("https://accounts.google.com/a/Login");
    observed_form_.username_element = ASCIIToUTF16("Email");
    observed_form_.username_element_renderer_id =
        autofill::FieldRendererId(100);
    observed_form_.password_element = ASCIIToUTF16("Passwd");
    observed_form_.password_element_renderer_id =
        autofill::FieldRendererId(101);
    observed_form_.submit_element = ASCIIToUTF16("signIn");
    observed_form_.signon_realm = "https://accounts.google.com";
    observed_form_.form_data.name = ASCIIToUTF16("the-form-name");

    saved_match_ = observed_form_;
    saved_match_.url = GURL("https://accounts.google.com/a/ServiceLoginAuth");
    saved_match_.action = GURL("https://accounts.google.com/a/ServiceLogin");
    saved_match_.username_value = ASCIIToUTF16("test@gmail.com");
    saved_match_.password_value = ASCIIToUTF16("test1");

    psl_saved_match_ = saved_match_;
    psl_saved_match_.is_public_suffix_match = true;
    psl_saved_match_.url =
        GURL("https://m.accounts.google.com/a/ServiceLoginAuth");
    psl_saved_match_.action = GURL("https://m.accounts.google.com/a/Login");
    psl_saved_match_.signon_realm = "https://m.accounts.google.com";

    metrics_recorder_ = base::MakeRefCounted<PasswordFormMetricsRecorder>(
        true, client_.GetUkmSourceId(), /*pref_service=*/nullptr);
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

  EXPECT_CALL(driver_, InformNoSavedCredentials(_));
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(0);

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

  EXPECT_CALL(driver_, InformNoSavedCredentials(_)).Times(0);
  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, FillPasswordForm(_)).WillOnce(SaveArg<0>(&fill_data));
  EXPECT_CALL(client_, PasswordWasAutofilled);

  LikelyFormFilling likely_form_filling = SendFillInformationToRenderer(
      &client_, &driver_, observed_form_, best_matches, federated_matches_,
      &saved_match_, metrics_recorder_.get());

  // On Android Touch To Fill will prevent autofilling credentials on page load.
#if defined(OS_ANDROID)
  EXPECT_EQ(LikelyFormFilling::kFillOnAccountSelect, likely_form_filling);
  EXPECT_TRUE(fill_data.wait_for_username);
#else
  EXPECT_EQ(LikelyFormFilling::kFillOnPageLoad, likely_form_filling);
  EXPECT_FALSE(fill_data.wait_for_username);
#endif

  // Check that the message to the renderer (i.e. |fill_data|) is filled
  // correctly.
  EXPECT_EQ(observed_form_.url, fill_data.url);
  EXPECT_EQ(observed_form_.username_element, fill_data.username_field.name);
  EXPECT_EQ(saved_match_.username_value, fill_data.username_field.value);
  EXPECT_EQ(observed_form_.password_element, fill_data.password_field.name);
  EXPECT_EQ(saved_match_.password_value, fill_data.password_field.value);

  // Check that information about non-preferred best matches is filled.
  ASSERT_EQ(1u, fill_data.additional_logins.size());
  EXPECT_EQ(another_saved_match.username_value,
            fill_data.additional_logins.begin()->username);
  EXPECT_EQ(another_saved_match.password_value,
            fill_data.additional_logins.begin()->password);
  // Realm is empty for non-psl match.
  EXPECT_TRUE(fill_data.additional_logins.begin()->realm.empty());
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
      observed_form.new_password_element_renderer_id =
          autofill::FieldRendererId(125);
    }
    if (!test_case.current_password_present) {
      observed_form.password_element.clear();
      observed_form.password_element_renderer_id = autofill::FieldRendererId();
    }

    PasswordFormFillData fill_data;
    EXPECT_CALL(driver_, FillPasswordForm(_)).WillOnce(SaveArg<0>(&fill_data));
    EXPECT_CALL(client_, PasswordWasAutofilled);

    LikelyFormFilling likely_form_filling = SendFillInformationToRenderer(
        &client_, &driver_, observed_form, best_matches, federated_matches_,
        &saved_match_, metrics_recorder_.get());

    // In all cases where a current password exists, fill on load should be
    // permitted. Otherwise, the renderer will not fill anyway and return
    // kFillOnAccountSelect.
    if (test_case.current_password_present) {
      // On Android Touch To Fill will prevent autofilling credentials on page
      // load.
#if defined(OS_ANDROID)
      EXPECT_EQ(LikelyFormFilling::kFillOnAccountSelect, likely_form_filling);
#else
      EXPECT_EQ(LikelyFormFilling::kFillOnPageLoad, likely_form_filling);
#endif
    } else {
      EXPECT_EQ(LikelyFormFilling::kFillOnAccountSelect, likely_form_filling);
    }
  }
}

// Test autofill when username and password are prefilled. Overwrite password
// if server side classification thought the username was a placeholder or the
// classification failed. Do not overwrite if username doesn't look like a
// placeholder.
// Skip for Android since it uses touch to fill (kAutofillTouchToFill), meaning
// placeholders will never be overwritten.
#if !defined(OS_ANDROID)
TEST_F(PasswordFormFillingTest, TestFillOnLoadSuggestionWithPrefill) {
  const struct {
    const char* description;
    bool username_may_use_prefilled_placeholder;
    bool server_side_classification_successful;
    LikelyFormFilling likely_form_filling;
  } kTestCases[] = {
      {
          .description = "Username not placeholder",
          .username_may_use_prefilled_placeholder = false,
          .server_side_classification_successful = true,
          .likely_form_filling = LikelyFormFilling::kFillOnAccountSelect,
      },
      {
          .description = "Username is placeholder",
          .username_may_use_prefilled_placeholder = true,
          .server_side_classification_successful = true,
          .likely_form_filling = LikelyFormFilling::kFillOnPageLoad,
      },
      {
          .description = "No server classification",
          .username_may_use_prefilled_placeholder = false,
          .server_side_classification_successful = false,
          .likely_form_filling = LikelyFormFilling::kFillOnPageLoad,
      },
  };
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);
    PasswordForm preferred_match = saved_match_;
    std::vector<const PasswordForm*> best_matches = {&preferred_match};

    PasswordForm observed_form = observed_form_;
    // Set username to match preferred match
    observed_form.username_value = preferred_match.username_value;
    // Set a different password than saved
    observed_form.password_value = ASCIIToUTF16("New Passwd");
    // Set classification results
    observed_form.server_side_classification_successful =
        test_case.server_side_classification_successful;
    observed_form.username_may_use_prefilled_placeholder =
        test_case.username_may_use_prefilled_placeholder;

    EXPECT_CALL(driver_, FillPasswordForm);
    EXPECT_CALL(client_, PasswordWasAutofilled);

    LikelyFormFilling likely_form_filling = SendFillInformationToRenderer(
        &client_, &driver_, observed_form, best_matches, federated_matches_,
        &preferred_match, metrics_recorder_.get());

    EXPECT_EQ(test_case.likely_form_filling, likely_form_filling);
  }
}
#endif

TEST_F(PasswordFormFillingTest, AutofillPSLMatch) {
  std::vector<const PasswordForm*> best_matches = {&psl_saved_match_};

  EXPECT_CALL(driver_, InformNoSavedCredentials(_)).Times(0);
  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, FillPasswordForm(_)).WillOnce(SaveArg<0>(&fill_data));
  EXPECT_CALL(client_, PasswordWasAutofilled);

  LikelyFormFilling likely_form_filling = SendFillInformationToRenderer(
      &client_, &driver_, observed_form_, best_matches, federated_matches_,
      &psl_saved_match_, metrics_recorder_.get());
  EXPECT_EQ(LikelyFormFilling::kFillOnAccountSelect, likely_form_filling);

  // Check that the message to the renderer (i.e. |fill_data|) is filled
  // correctly.
  EXPECT_EQ(observed_form_.url, fill_data.url);
  EXPECT_TRUE(fill_data.wait_for_username);
  EXPECT_EQ(psl_saved_match_.signon_realm, fill_data.preferred_realm);
  EXPECT_EQ(observed_form_.username_element, fill_data.username_field.name);
  EXPECT_EQ(saved_match_.username_value, fill_data.username_field.value);
  EXPECT_EQ(observed_form_.password_element, fill_data.password_field.name);
  EXPECT_EQ(saved_match_.password_value, fill_data.password_field.value);
}

TEST_F(PasswordFormFillingTest, NoAutofillOnHttp) {
  PasswordForm observed_http_form = observed_form_;
  observed_http_form.url = GURL("http://accounts.google.com/a/LoginAuth");
  observed_http_form.action = GURL("http://accounts.google.com/a/Login");
  observed_http_form.signon_realm = "http://accounts.google.com";

  PasswordForm saved_http_match = saved_match_;
  saved_http_match.url = GURL("http://accounts.google.com/a/ServiceLoginAuth");
  saved_http_match.action = GURL("http://accounts.google.com/a/ServiceLogin");
  saved_http_match.signon_realm = "http://accounts.google.com";

  ASSERT_FALSE(GURL(saved_http_match.signon_realm).SchemeIsCryptographic());
  std::vector<const PasswordForm*> best_matches = {&saved_http_match};

  EXPECT_CALL(client_, IsCommittedMainFrameSecure).WillOnce(Return(false));
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

// Tests that the when there is a single preferred match, and no extra
// matches, the PasswordFormFillData is filled in correctly.
TEST(PasswordFormFillDataTest, TestSinglePreferredMatch) {
  // Create the current form on the page.
  PasswordForm form_on_page;
  form_on_page.url = GURL("https://foo.com/");
  form_on_page.action = GURL("https://foo.com/login");
  form_on_page.username_element = ASCIIToUTF16("username");
  form_on_page.username_value = ASCIIToUTF16(kPreferredUsername);
  form_on_page.password_element = ASCIIToUTF16("password");
  form_on_page.password_value = ASCIIToUTF16(kPreferredPassword);
  form_on_page.submit_element = ASCIIToUTF16("");
  form_on_page.signon_realm = "https://foo.com/";
  form_on_page.scheme = PasswordForm::Scheme::kHtml;

  // Create an exact match in the database.
  PasswordForm preferred_match;
  preferred_match.url = GURL("https://foo.com/");
  preferred_match.action = GURL("https://foo.com/login");
  preferred_match.username_element = ASCIIToUTF16("username");
  preferred_match.username_value = ASCIIToUTF16(kPreferredUsername);
  preferred_match.password_element = ASCIIToUTF16("password");
  preferred_match.password_value = ASCIIToUTF16(kPreferredPassword);
  preferred_match.submit_element = ASCIIToUTF16("");
  preferred_match.signon_realm = "https://foo.com/";
  preferred_match.scheme = PasswordForm::Scheme::kHtml;

  std::vector<const PasswordForm*> matches;

  PasswordFormFillData result =
      CreatePasswordFormFillData(form_on_page, matches, preferred_match, true);

  // |wait_for_username| should reflect the |wait_for_username| argument passed
  // to the constructor, which in this case is true.
  EXPECT_TRUE(result.wait_for_username);
  // The preferred realm should be empty since it's the same as the realm of
  // the form.
  EXPECT_EQ(std::string(), result.preferred_realm);

  PasswordFormFillData result2 =
      CreatePasswordFormFillData(form_on_page, matches, preferred_match, false);

  // |wait_for_username| should reflect the |wait_for_username| argument passed
  // to the constructor, which in this case is false.
  EXPECT_FALSE(result2.wait_for_username);
}

// Tests that constructing a PasswordFormFillData behaves correctly when there
// is a preferred match that was found using public suffix matching, an
// additional result that also used public suffix matching, and a third result
// that was found without using public suffix matching.
TEST(PasswordFormFillDataTest, TestPublicSuffixDomainMatching) {
  // Create the current form on the page.
  PasswordForm form_on_page;
  form_on_page.url = GURL("https://foo.com/");
  form_on_page.action = GURL("https://foo.com/login");
  form_on_page.username_element = ASCIIToUTF16("username");
  form_on_page.username_value = ASCIIToUTF16(kPreferredUsername);
  form_on_page.password_element = ASCIIToUTF16("password");
  form_on_page.password_value = ASCIIToUTF16(kPreferredPassword);
  form_on_page.submit_element = ASCIIToUTF16("");
  form_on_page.signon_realm = "https://foo.com/";
  form_on_page.scheme = PasswordForm::Scheme::kHtml;

  // Create a match from the database that matches using public suffix.
  PasswordForm preferred_match;
  preferred_match.url = GURL("https://mobile.foo.com/");
  preferred_match.action = GURL("https://mobile.foo.com/login");
  preferred_match.username_element = ASCIIToUTF16("username");
  preferred_match.username_value = ASCIIToUTF16(kPreferredUsername);
  preferred_match.password_element = ASCIIToUTF16("password");
  preferred_match.password_value = ASCIIToUTF16(kPreferredPassword);
  preferred_match.submit_element = ASCIIToUTF16("");
  preferred_match.signon_realm = "https://foo.com/";
  preferred_match.is_public_suffix_match = true;
  preferred_match.scheme = PasswordForm::Scheme::kHtml;

  // Create a match that matches exactly, so |is_public_suffix_match| has a
  // default value false.
  PasswordForm exact_match;
  exact_match.url = GURL("https://foo.com/");
  exact_match.action = GURL("https://foo.com/login");
  exact_match.username_element = ASCIIToUTF16("username");
  exact_match.username_value = ASCIIToUTF16("test1@gmail.com");
  exact_match.password_element = ASCIIToUTF16("password");
  exact_match.password_value = ASCIIToUTF16(kPreferredPassword);
  exact_match.submit_element = ASCIIToUTF16("");
  exact_match.signon_realm = "https://foo.com/";
  exact_match.scheme = PasswordForm::Scheme::kHtml;

  // Create a match that was matched using public suffix, so
  // |is_public_suffix_match| == true.
  PasswordForm public_suffix_match;
  public_suffix_match.url = GURL("https://foo.com/");
  public_suffix_match.action = GURL("https://foo.com/login");
  public_suffix_match.username_element = ASCIIToUTF16("username");
  public_suffix_match.username_value = ASCIIToUTF16("test2@gmail.com");
  public_suffix_match.password_element = ASCIIToUTF16("password");
  public_suffix_match.password_value = ASCIIToUTF16(kPreferredPassword);
  public_suffix_match.submit_element = ASCIIToUTF16("");
  public_suffix_match.is_public_suffix_match = true;
  public_suffix_match.signon_realm = "https://foo.com/";
  public_suffix_match.scheme = PasswordForm::Scheme::kHtml;

  // Add one exact match and one public suffix match.
  std::vector<const PasswordForm*> matches = {&exact_match,
                                              &public_suffix_match};

  PasswordFormFillData result =
      CreatePasswordFormFillData(form_on_page, matches, preferred_match, true);
  EXPECT_TRUE(result.wait_for_username);
  // The preferred realm should match the signon realm from the
  // preferred match so the user can see where the result came from.
  EXPECT_EQ(preferred_match.signon_realm, result.preferred_realm);

  // The realm of the exact match should be empty.
  PasswordFormFillData::LoginCollection::const_iterator iter =
      FindPasswordByUsername(result.additional_logins,
                             exact_match.username_value);
  EXPECT_EQ(std::string(), iter->realm);

  // The realm of the public suffix match should be set to the original signon
  // realm so the user can see where the result came from.
  iter = FindPasswordByUsername(result.additional_logins,
                                public_suffix_match.username_value);
  EXPECT_EQ(iter->realm, public_suffix_match.signon_realm);
}

// Tests that the constructing a PasswordFormFillData behaves correctly when
// there is a preferred match that was found using affiliation based matching,
// an additional result that also used affiliation based matching, and a third
// result that was found without using affiliation based matching.
TEST(PasswordFormFillDataTest, TestAffiliationMatch) {
  // Create the current form on the page.
  PasswordForm form_on_page;
  form_on_page.url = GURL("https://foo.com/");
  form_on_page.action = GURL("https://foo.com/login");
  form_on_page.username_element = ASCIIToUTF16("username");
  form_on_page.username_value = ASCIIToUTF16(kPreferredUsername);
  form_on_page.password_element = ASCIIToUTF16("password");
  form_on_page.password_value = ASCIIToUTF16(kPreferredPassword);
  form_on_page.submit_element = ASCIIToUTF16("");
  form_on_page.signon_realm = "https://foo.com/";
  form_on_page.scheme = PasswordForm::Scheme::kHtml;

  // Create a match from the database that matches using affiliation.
  PasswordForm preferred_match;
  preferred_match.url = GURL("android://hash@foo.com/");
  preferred_match.username_value = ASCIIToUTF16(kPreferredUsername);
  preferred_match.password_value = ASCIIToUTF16(kPreferredPassword);
  preferred_match.signon_realm = "android://hash@foo.com/";
  preferred_match.is_affiliation_based_match = true;

  // Create a match that matches exactly, so |is_affiliation_based_match| has a
  // default value false.
  PasswordForm exact_match;
  exact_match.url = GURL("https://foo.com/");
  exact_match.action = GURL("https://foo.com/login");
  exact_match.username_element = ASCIIToUTF16("username");
  exact_match.username_value = ASCIIToUTF16("test1@gmail.com");
  exact_match.password_element = ASCIIToUTF16("password");
  exact_match.password_value = ASCIIToUTF16(kPreferredPassword);
  exact_match.submit_element = ASCIIToUTF16("");
  exact_match.signon_realm = "https://foo.com/";
  exact_match.scheme = PasswordForm::Scheme::kHtml;

  // Create a match that was matched using public suffix, so
  // |is_public_suffix_match| == true.
  PasswordForm affiliated_match;
  affiliated_match.url = GURL("android://hash@foo1.com/");
  affiliated_match.username_value = ASCIIToUTF16("test2@gmail.com");
  affiliated_match.password_value = ASCIIToUTF16(kPreferredPassword);
  affiliated_match.is_affiliation_based_match = true;
  affiliated_match.signon_realm = "https://foo1.com/";
  affiliated_match.scheme = PasswordForm::Scheme::kHtml;

  // Add one exact match and one affiliation based match.
  std::vector<const PasswordForm*> matches = {&exact_match, &affiliated_match};

  PasswordFormFillData result =
      CreatePasswordFormFillData(form_on_page, matches, preferred_match, false);
  EXPECT_FALSE(result.wait_for_username);
  // The preferred realm should match the signon realm from the
  // preferred match so the user can see where the result came from.
  EXPECT_EQ(preferred_match.signon_realm, result.preferred_realm);

  // The realm of the exact match should be empty.
  PasswordFormFillData::LoginCollection::const_iterator iter =
      FindPasswordByUsername(result.additional_logins,
                             exact_match.username_value);
  EXPECT_EQ(std::string(), iter->realm);

  // The realm of the affiliation based match should be set to the original
  // signon realm so the user can see where the result came from.
  iter = FindPasswordByUsername(result.additional_logins,
                                affiliated_match.username_value);
  EXPECT_EQ(iter->realm, affiliated_match.signon_realm);
}

// Tests that renderer ids are passed correctly.
TEST(PasswordFormFillDataTest, RendererIDs) {
  // Create the current form on the page.
  PasswordForm form_on_page;
  form_on_page.url = GURL("https://foo.com/");
  form_on_page.action = GURL("https://foo.com/login");
  form_on_page.username_element = ASCIIToUTF16("username");
  form_on_page.password_element = ASCIIToUTF16("password");
  form_on_page.username_may_use_prefilled_placeholder = true;
  form_on_page.server_side_classification_successful = true;

  // Create an exact match in the database.
  PasswordForm preferred_match = form_on_page;
  preferred_match.username_value = ASCIIToUTF16(kPreferredUsername);
  preferred_match.password_value = ASCIIToUTF16(kPreferredPassword);

  // Set renderer id related fields.
  FormData form_data;
  form_data.unique_renderer_id = FormRendererId(42);
  form_data.is_form_tag = true;
  form_on_page.form_data = form_data;
  form_on_page.username_element_renderer_id = FieldRendererId(123);
  form_on_page.password_element_renderer_id = FieldRendererId(456);

  PasswordFormFillData result =
      CreatePasswordFormFillData(form_on_page, {}, preferred_match, true);

  EXPECT_EQ(form_data.unique_renderer_id, result.form_renderer_id);
  EXPECT_EQ(form_on_page.username_element_renderer_id,
            result.username_field.unique_renderer_id);
  EXPECT_EQ(form_on_page.password_element_renderer_id,
            result.password_field.unique_renderer_id);
  EXPECT_TRUE(result.username_may_use_prefilled_placeholder);
}

// Tests that nor username nor password fields are set when password element is
// not found.
TEST(PasswordFormFillDataTest, NoPasswordElement) {
  // Create the current form on the page.
  PasswordForm form_on_page;
  form_on_page.url = GURL("https://foo.com/");
  form_on_page.username_element_renderer_id = FieldRendererId(123);
  // Set no password element.
  form_on_page.password_element_renderer_id = FieldRendererId();
  form_on_page.new_password_element_renderer_id = FieldRendererId(456);

  // Create an exact match in the database.
  PasswordForm preferred_match = form_on_page;
  preferred_match.username_value = ASCIIToUTF16(kPreferredUsername);
  preferred_match.password_value = ASCIIToUTF16(kPreferredPassword);

  FormData form_data;
  form_data.unique_renderer_id = FormRendererId(42);
  form_data.is_form_tag = true;
  form_on_page.form_data = form_data;

  PasswordFormFillData result = CreatePasswordFormFillData(
      form_on_page, {} /* matches */, preferred_match, true);

  // Check that nor username nor password fields are set.
  EXPECT_TRUE(result.username_field.unique_renderer_id.is_null());
  EXPECT_TRUE(result.password_field.unique_renderer_id.is_null());
}

// Tests that matches are retained without duplicates.
TEST(PasswordFormFillDataTest, DeduplicatesFillData) {
  // Create the current form on the page.
  PasswordForm form;
  form.username_element = ASCIIToUTF16("username");
  form.password_element = ASCIIToUTF16("password");

  // Create an exact match in the database.
  PasswordForm preferred_match = form;
  preferred_match.username_value = ASCIIToUTF16(kPreferredUsername);
  preferred_match.password_value = ASCIIToUTF16(kPreferredPassword);
  preferred_match.in_store = Store::kProfileStore;

  // Create two discarded and one retained duplicate.
  const PasswordForm duplicate_of_preferred =
      CreateForm(kPreferredUsername, kPreferredPassword, Store::kProfileStore);
  const PasswordForm account_duplicate_of_preferred =
      CreateForm(kPreferredUsername, kPreferredPassword, Store::kAccountStore);
  const PasswordForm non_duplicate_of_preferred = CreateForm(
      kPreferredUsername, kPreferredAlternatePassword, Store::kAccountStore);

  // Create a local password and its discarded duplicate.
  const PasswordForm local = CreateForm(
      kDuplicateLocalUsername, kDuplicateLocalPassword, Store::kProfileStore);
  const PasswordForm duplicate_of_local = local;

  // Create a synced password and its discarded local duplicate.
  const PasswordForm remote =
      CreateForm(kSyncedUsername, kSyncedPassword, Store::kProfileStore);
  const PasswordForm duplicate_of_remote =
      CreateForm(kSyncedUsername, kSyncedPassword, Store::kAccountStore);

  PasswordFormFillData result = CreatePasswordFormFillData(
      form,
      {&duplicate_of_preferred, &account_duplicate_of_preferred,
       &non_duplicate_of_preferred, &local, &duplicate_of_local, &remote,
       &duplicate_of_remote},
      preferred_match, true);

  EXPECT_EQ(preferred_match.username_value, result.username_field.value);
  EXPECT_EQ(preferred_match.password_value, result.password_field.value);
  EXPECT_TRUE(result.uses_account_store);
  EXPECT_THAT(
      result.additional_logins,
      testing::ElementsAre(
          IsLogin(kPreferredUsername, kPreferredAlternatePassword, true),
          IsLogin(kDuplicateLocalUsername, kDuplicateLocalPassword, false),
          IsLogin(kSyncedUsername, kSyncedPassword, true)));
}

}  // namespace password_manager
