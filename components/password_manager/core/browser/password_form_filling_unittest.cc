// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form_filling.h"

#include <map>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/mock_webauthn_credentials_delegate.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class PrefService;

using autofill::FieldRendererId;
using autofill::FormData;
using autofill::FormRendererId;
using autofill::PasswordFormFillData;
using testing::_;
using testing::Return;
using testing::SaveArg;
using url::Origin;
using Store = password_manager::PasswordForm::Store;

namespace password_manager {
namespace {

constexpr char16_t kPreferredUsername[] = u"test@gmail.com";
constexpr char16_t kPreferredPassword[] = u"password";

class MockPasswordManagerDriver : public StubPasswordManagerDriver {
 public:
  MOCK_METHOD(int, GetId, (), (const, override));
  MOCK_METHOD(void,
              SetPasswordFillData,
              (const PasswordFormFillData&),
              (override));
  MOCK_METHOD(void, InformNoSavedCredentials, (bool), (override));
};

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MOCK_METHOD(void,
              PasswordWasAutofilled,
              (base::span<const PasswordForm>,
               const Origin&,
               (base::span<const PasswordForm>),
               bool was_autofilled_on_pageload),
              (override));
  MOCK_METHOD(bool,
              IsSavingAndFillingEnabled,
              (const GURL&),
              (const, override));
  MOCK_METHOD(bool, IsCommittedMainFrameSecure, (), (const, override));
  MOCK_METHOD(MockWebAuthnCredentialsDelegate*,
              GetWebAuthnCredentialsDelegateForDriver,
              (PasswordManagerDriver*),
              (override));
  MOCK_METHOD(MockPasswordFeatureManager*,
              GetPasswordFeatureManager,
              (),
              (const, override));
  MOCK_METHOD(Origin, GetLastCommittedOrigin, (), (const, override));
};

// Matcher for PasswordAndMetadata.
MATCHER_P3(IsLogin, username, password, uses_account_store, std::string()) {
  return arg.username == username && arg.password == password &&
         arg.uses_account_store == uses_account_store;
}

PasswordFormFillData::LoginCollection::const_iterator FindPasswordByUsername(
    const std::vector<autofill::PasswordAndMetadata>& logins,
    const std::u16string& username) {
  return base::ranges::find(logins, username,
                            &autofill::PasswordAndMetadata::username_value);
}

}  // namespace

class PasswordFormFillingTest : public testing::Test {
 public:
  PasswordFormFillingTest() {
    ON_CALL(client_, IsCommittedMainFrameSecure()).WillByDefault(Return(true));
    ON_CALL(client_, GetLastCommittedOrigin())
        .WillByDefault(
            Return(Origin::Create(GURL("https://accounts.google.com"))));

    observed_form_.url = GURL("https://accounts.google.com/a/LoginAuth");
    observed_form_.action = GURL("https://accounts.google.com/a/Login");
    observed_form_.username_element = u"Email";
    observed_form_.username_element_renderer_id =
        autofill::FieldRendererId(100);
    observed_form_.password_element = u"Passwd";
    observed_form_.password_element_renderer_id =
        autofill::FieldRendererId(101);
    observed_form_.submit_element = u"signIn";
    observed_form_.signon_realm = "https://accounts.google.com";
    observed_form_.form_data.set_name(u"the-form-name");

    saved_match_ = observed_form_;
    saved_match_.url = GURL("https://accounts.google.com/a/ServiceLoginAuth");
    saved_match_.action = GURL("https://accounts.google.com/a/ServiceLogin");
    saved_match_.username_value = u"test@gmail.com";
    saved_match_.password_value = u"test1";
    saved_match_.match_type = PasswordForm::MatchType::kExact;

    psl_saved_match_ = saved_match_;
    psl_saved_match_.match_type = PasswordForm::MatchType::kPSL;
    psl_saved_match_.url =
        GURL("https://m.accounts.google.com/a/ServiceLoginAuth");
    psl_saved_match_.action = GURL("https://m.accounts.google.com/a/Login");
    psl_saved_match_.signon_realm = "https://m.accounts.google.com";

    metrics_recorder_ = base::MakeRefCounted<PasswordFormMetricsRecorder>(
        true, client_.GetUkmSourceId(), /*pref_service=*/nullptr);

    ON_CALL(client_, GetWebAuthnCredentialsDelegateForDriver)
        .WillByDefault(Return(&webauthn_credentials_delegate_));
    ON_CALL(client_, GetPasswordFeatureManager)
        .WillByDefault(Return(&feature_manager_));
  }

 protected:
  MockPasswordManagerDriver driver_;
  MockPasswordManagerClient client_;
  PasswordForm observed_form_;
  PasswordForm saved_match_;
  PasswordForm psl_saved_match_;
  scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder_;
  const std::vector<PasswordForm> federated_matches_;
  MockWebAuthnCredentialsDelegate webauthn_credentials_delegate_;
  testing::NiceMock<MockPasswordFeatureManager> feature_manager_;
};

TEST_F(PasswordFormFillingTest, NoSavedCredentials) {
  std::vector<PasswordForm> best_matches;

  EXPECT_CALL(driver_, InformNoSavedCredentials(_));
  EXPECT_CALL(driver_, SetPasswordFillData(_)).Times(0);

  LikelyFormFilling likely_form_filling = SendFillInformationToRenderer(
      &client_, &driver_, observed_form_, best_matches, federated_matches_,
      nullptr, metrics_recorder_.get(),
      /*webauthn_suggestions_available=*/false,
      /*suggestion_banned_fields=*/{});
  EXPECT_EQ(LikelyFormFilling::kNoFilling, likely_form_filling);
}

TEST_F(PasswordFormFillingTest, Autofill) {
  std::vector<PasswordForm> best_matches;
  best_matches.push_back(saved_match_);
  PasswordForm another_saved_match = saved_match_;
  another_saved_match.username_value += u"1";
  another_saved_match.password_value += u"1";
  best_matches.push_back(another_saved_match);

  EXPECT_CALL(driver_, InformNoSavedCredentials(_)).Times(0);
  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, SetPasswordFillData(_)).WillOnce(SaveArg<0>(&fill_data));
  EXPECT_CALL(client_, PasswordWasAutofilled);
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_CALL(feature_manager_, IsBiometricAuthenticationBeforeFillingEnabled)
      .WillOnce(Return(true));
#endif

  LikelyFormFilling likely_form_filling = SendFillInformationToRenderer(
      &client_, &driver_, observed_form_, best_matches, federated_matches_,
      &saved_match_, metrics_recorder_.get(),
      /*webauthn_suggestions_available=*/false,
      /*suggestion_banned_fields=*/{});

  // On Android, Mac and Win authentication will prevent autofilling credentials
  // on page load. On iOS Reauth is always required.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(LikelyFormFilling::kFillOnAccountSelect, likely_form_filling);
  EXPECT_TRUE(fill_data.wait_for_username);
#else
  EXPECT_EQ(LikelyFormFilling::kFillOnPageLoad, likely_form_filling);
  EXPECT_FALSE(fill_data.wait_for_username);
#endif

  // Check that the message to the renderer (i.e. |fill_data|) is filled
  // correctly.
  EXPECT_EQ(observed_form_.url, fill_data.url);
  EXPECT_EQ(saved_match_.username_value,
            fill_data.preferred_login.username_value);
  EXPECT_EQ(saved_match_.password_value,
            fill_data.preferred_login.password_value);

  // Check that information about non-preferred best matches is filled.
  ASSERT_EQ(1u, fill_data.additional_logins.size());
  EXPECT_EQ(another_saved_match.username_value,
            fill_data.additional_logins.begin()->username_value);
  EXPECT_EQ(another_saved_match.password_value,
            fill_data.additional_logins.begin()->password_value);
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
    std::vector<PasswordForm> best_matches = {saved_match_};

    PasswordForm observed_form = observed_form_;
    if (test_case.new_password_present) {
      observed_form.new_password_element = u"New Passwd";
      observed_form.new_password_element_renderer_id =
          autofill::FieldRendererId(125);
    }
    if (!test_case.current_password_present) {
      observed_form.password_element.clear();
      observed_form.password_element_renderer_id = autofill::FieldRendererId();
    }

    PasswordFormFillData fill_data;
    EXPECT_CALL(driver_, SetPasswordFillData(_))
        .WillOnce(SaveArg<0>(&fill_data));
    EXPECT_CALL(client_, PasswordWasAutofilled);
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
    EXPECT_CALL(feature_manager_, IsBiometricAuthenticationBeforeFillingEnabled)
        .WillOnce(Return(true));
#endif

    LikelyFormFilling likely_form_filling = SendFillInformationToRenderer(
        &client_, &driver_, observed_form, best_matches, federated_matches_,
        &saved_match_, metrics_recorder_.get(),
        /*webauthn_suggestions_available=*/false,
        /*suggestion_banned_fields=*/{});

    // In all cases where a current password exists, fill on load should be
    // permitted. Otherwise, the renderer will not fill anyway and return
    // kFillOnAccountSelect.
    if (test_case.current_password_present) {
      // On Android, Mac and Win authentication will prevent autofilling
      // credentials on page load. On iOS Reauth is always required.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
      EXPECT_EQ(LikelyFormFilling::kFillOnAccountSelect, likely_form_filling);
#else
      EXPECT_EQ(LikelyFormFilling::kFillOnPageLoad, likely_form_filling);
#endif
    } else {
      EXPECT_EQ(LikelyFormFilling::kFillOnAccountSelect, likely_form_filling);
    }
  }
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(PasswordFormFillingTest, FillWithOnlyWebAuthnCredentials) {
  observed_form_.accepts_webauthn_credentials = true;
  std::vector<PasswordForm> best_matches = {saved_match_};

  EXPECT_CALL(client_, PasswordWasAutofilled);
  LikelyFormFilling likely_form_filling = SendFillInformationToRenderer(
      &client_, &driver_, observed_form_, best_matches, federated_matches_,
      &saved_match_, metrics_recorder_.get(),
      /*webauthn_suggestions_available=*/false,
      /*suggestion_banned_fields=*/{});
  EXPECT_EQ(LikelyFormFilling::kFillOnAccountSelect, likely_form_filling);
}
#endif

// Test autofill when username and password are prefilled. Overwrite password
// if server side classification thought the username was a placeholder or the
// classification failed. Do not overwrite if username doesn't look like a
// placeholder.
// Skip for Android and iOS since it uses touch to fill, meaning placeholders
// will never be overwritten.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
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
    std::vector<PasswordForm> best_matches = {preferred_match};

    PasswordForm observed_form = observed_form_;
    // Set username to match preferred match
    observed_form.username_value = preferred_match.username_value;
    // Set a different password than saved
    observed_form.password_value = u"New Passwd";
    // Set classification results
    observed_form.server_side_classification_successful =
        test_case.server_side_classification_successful;
    observed_form.username_may_use_prefilled_placeholder =
        test_case.username_may_use_prefilled_placeholder;

    EXPECT_CALL(driver_, SetPasswordFillData);
    EXPECT_CALL(client_, PasswordWasAutofilled);

    LikelyFormFilling likely_form_filling = SendFillInformationToRenderer(
        &client_, &driver_, observed_form, best_matches, federated_matches_,
        &preferred_match, metrics_recorder_.get(),
        /*webauthn_suggestions_available=*/false,
        /*suggestion_banned_fields=*/{});

    EXPECT_EQ(test_case.likely_form_filling, likely_form_filling);
  }
}
#endif

TEST_F(PasswordFormFillingTest, AutofillPSLMatch) {
  std::vector<PasswordForm> best_matches = {psl_saved_match_};

  EXPECT_CALL(driver_, InformNoSavedCredentials(_)).Times(0);
  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, SetPasswordFillData(_)).WillOnce(SaveArg<0>(&fill_data));
  EXPECT_CALL(client_, PasswordWasAutofilled);

  LikelyFormFilling likely_form_filling = SendFillInformationToRenderer(
      &client_, &driver_, observed_form_, best_matches, federated_matches_,
      &psl_saved_match_, metrics_recorder_.get(),
      /*webauthn_suggestions_available=*/false,
      /*suggestion_banned_fields=*/{});
  EXPECT_EQ(LikelyFormFilling::kFillOnAccountSelect, likely_form_filling);

  // Check that the message to the renderer (i.e. |fill_data|) is filled
  // correctly.
  EXPECT_EQ(observed_form_.url, fill_data.url);
  EXPECT_TRUE(fill_data.wait_for_username);
  EXPECT_EQ(observed_form_.username_element_renderer_id,
            fill_data.username_element_renderer_id);
  EXPECT_EQ(observed_form_.password_element_renderer_id,
            fill_data.password_element_renderer_id);
  EXPECT_EQ(psl_saved_match_.signon_realm, fill_data.preferred_login.realm);
  EXPECT_EQ(saved_match_.username_value,
            fill_data.preferred_login.username_value);
  EXPECT_EQ(saved_match_.password_value,
            fill_data.preferred_login.password_value);
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

  ON_CALL(client_, GetLastCommittedOrigin)
      .WillByDefault(
          Return(Origin::Create(GURL(observed_http_form.signon_realm))));

  ASSERT_FALSE(GURL(saved_http_match.signon_realm).SchemeIsCryptographic());
  std::vector<PasswordForm> best_matches = {saved_http_match};

#if !BUILDFLAG(IS_IOS) && !defined(ANDROID)
  EXPECT_CALL(client_, IsCommittedMainFrameSecure).WillOnce(Return(false));
#endif
  LikelyFormFilling likely_form_filling = SendFillInformationToRenderer(
      &client_, &driver_, observed_http_form, best_matches, federated_matches_,
      &saved_http_match, metrics_recorder_.get(),
      /*webauthn_suggestions_available=*/false,
      /*suggestion_banned_fields=*/{});
  EXPECT_EQ(LikelyFormFilling::kFillOnAccountSelect, likely_form_filling);
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(PasswordFormFillingTest, TouchToFill) {
  std::vector<PasswordForm> best_matches = {saved_match_};

  LikelyFormFilling likely_form_filling = SendFillInformationToRenderer(
      &client_, &driver_, observed_form_, best_matches, federated_matches_,
      &saved_match_, metrics_recorder_.get(),
      /*webauthn_suggestions_available=*/false,
      /*suggestion_banned_fields=*/{});
  EXPECT_EQ(LikelyFormFilling::kFillOnAccountSelect, likely_form_filling);
}
#endif

TEST_F(PasswordFormFillingTest, AutofillAffiliatedWebMatch) {
  base::HistogramTester histogram_tester;
  // Create a match from the database that matches using affiliation.
  PasswordForm affiliated_match;
  affiliated_match.url = GURL("https://fooo.com/");
  affiliated_match.username_value = u"test@gmail.com";
  affiliated_match.password_value = u"test1";
  affiliated_match.signon_realm = "https://fooo.com/";
  affiliated_match.match_type = PasswordForm::MatchType::kAffiliated;

  std::vector<PasswordForm> best_matches = {affiliated_match};

  EXPECT_CALL(driver_, InformNoSavedCredentials).Times(0);
  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, SetPasswordFillData).WillOnce(SaveArg<0>(&fill_data));
  EXPECT_CALL(client_, PasswordWasAutofilled);

  LikelyFormFilling likely_form_filling = SendFillInformationToRenderer(
      &client_, &driver_, observed_form_, best_matches, federated_matches_,
      &affiliated_match, metrics_recorder_.get(),
      /*webauthn_suggestions_available=*/false,
      /*suggestion_banned_fields=*/{});
  EXPECT_EQ(LikelyFormFilling::kFillOnAccountSelect, likely_form_filling);

  // Check that the message to the renderer (i.e. |fill_data|) is filled
  // correctly.
  EXPECT_EQ(observed_form_.url, fill_data.url);
  EXPECT_TRUE(fill_data.wait_for_username);
  EXPECT_EQ(affiliated_match.signon_realm, fill_data.preferred_login.realm);
  EXPECT_EQ(saved_match_.username_value,
            fill_data.preferred_login.username_value);
  EXPECT_EQ(saved_match_.password_value,
            fill_data.preferred_login.password_value);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.MatchedFormType",
      PasswordFormMetricsRecorder::MatchedFormType::kAffiliatedWebsites, 1);
}

TEST_F(PasswordFormFillingTest,
       AccountStorePromoWhenNoCredentialSavedAndSavingAndFillingEnabled) {
  ON_CALL(client_, IsSavingAndFillingEnabled).WillByDefault(Return(true));
  ON_CALL(*client_.GetPasswordFeatureManager(), ShouldShowAccountStorageOptIn())
      .WillByDefault(Return(true));

  std::vector<PasswordForm> best_matches;
  EXPECT_CALL(driver_, InformNoSavedCredentials(
                           /*should_show_popup_without_passwords=*/true));
  SendFillInformationToRenderer(&client_, &driver_, observed_form_,
                                best_matches, federated_matches_, nullptr,
                                metrics_recorder_.get(),
                                /*webauthn_suggestions_available=*/false,
                                /*suggestion_banned_fields=*/{});
}

TEST_F(PasswordFormFillingTest,
       NoAccountStorePromoWhenNoCredentialSavedAndSavingAndFillingDisabled) {
  ON_CALL(client_, IsSavingAndFillingEnabled).WillByDefault(Return(false));
  ON_CALL(*client_.GetPasswordFeatureManager(), ShouldShowAccountStorageOptIn())
      .WillByDefault(Return(true));

  std::vector<PasswordForm> best_matches;
  EXPECT_CALL(driver_, InformNoSavedCredentials(
                           /*should_show_popup_without_passwords=*/false));
  SendFillInformationToRenderer(&client_, &driver_, observed_form_,
                                best_matches, federated_matches_, nullptr,
                                metrics_recorder_.get(),
                                /*webauthn_suggestions_available=*/false,
                                /*suggestion_banned_fields=*/{});
}

// Exclude Android and iOS, because there credentials are not filled on
// the page load in any case.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(PasswordFormFillingTest, NoFillOnPageloadInCrossOriginIframe) {
  base::HistogramTester histogram_tester;

  observed_form_.url = GURL("https://some_website.com");
  saved_match_.url = GURL("https://some_website.com");
  ON_CALL(client_, GetLastCommittedOrigin)
      .WillByDefault(
          Return(Origin::Create(GURL("https://another_website.com"))));

  std::vector<PasswordForm> best_matches = {saved_match_};
  const std::vector<PasswordForm> federated_matches = {};

  LikelyFormFilling likely_form_filling = SendFillInformationToRenderer(
      &client_, &driver_, observed_form_, best_matches, federated_matches,
      &saved_match_, metrics_recorder_.get(),
      /*webauthn_suggestions_available=*/false,
      /*suggestion_banned_fields=*/{});
  EXPECT_EQ(LikelyFormFilling::kFillOnAccountSelect, likely_form_filling);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.FirstWaitForUsernameReason",
      PasswordFormMetricsRecorder::WaitForUsernameReason::kCrossOriginIframe,
      1);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Tests that the when there is a single preferred match, and no extra
// matches, the PasswordFormFillData is filled in correctly.
TEST(PasswordFormFillDataTest, TestSinglePreferredMatch) {
  // Create the current form on the page.
  PasswordForm form_on_page;
  form_on_page.url = GURL("https://foo.com/");
  form_on_page.action = GURL("https://foo.com/login");
  form_on_page.username_element = u"username";
  form_on_page.username_value = kPreferredUsername;
  form_on_page.password_element = u"password";
  form_on_page.password_value = kPreferredPassword;
  form_on_page.submit_element = u"";
  form_on_page.signon_realm = "https://foo.com/";
  form_on_page.scheme = PasswordForm::Scheme::kHtml;

  // Create an exact match in the database.
  PasswordForm preferred_match;
  preferred_match.url = GURL("https://foo.com/");
  preferred_match.action = GURL("https://foo.com/login");
  preferred_match.username_element = u"username";
  preferred_match.username_value = kPreferredUsername;
  preferred_match.password_element = u"password";
  preferred_match.password_value = kPreferredPassword;
  preferred_match.submit_element = u"";
  preferred_match.signon_realm = "https://foo.com/";
  preferred_match.scheme = PasswordForm::Scheme::kHtml;
  preferred_match.match_type = PasswordForm::MatchType::kExact;

  Origin page_origin = Origin::Create(GURL("https://foo.com/"));

  std::vector<PasswordForm> matches;

  PasswordFormFillData result = CreatePasswordFormFillData(
      form_on_page, matches, preferred_match, page_origin,
      /*wait_for_username=*/true, /*suggestion_banned_fields=*/{});

  // |wait_for_username| should reflect the |wait_for_username| argument passed
  // to the constructor, which in this case is true.
  EXPECT_TRUE(result.wait_for_username);
  // The preferred realm should be empty since it's the same as the realm of
  // the form.
  EXPECT_EQ(std::string(), result.preferred_login.realm);

  PasswordFormFillData result2 = CreatePasswordFormFillData(
      form_on_page, matches, preferred_match, page_origin,
      /*wait_for_username=*/false, /*suggestion_banned_fields=*/{});

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
  form_on_page.username_element = u"username";
  form_on_page.username_value = kPreferredUsername;
  form_on_page.password_element = u"password";
  form_on_page.password_value = kPreferredPassword;
  form_on_page.submit_element = u"";
  form_on_page.signon_realm = "https://foo.com/";
  form_on_page.scheme = PasswordForm::Scheme::kHtml;

  // Create a match from the database that matches using public suffix.
  PasswordForm preferred_match;
  preferred_match.url = GURL("https://mobile.foo.com/");
  preferred_match.action = GURL("https://mobile.foo.com/login");
  preferred_match.username_element = u"username";
  preferred_match.username_value = kPreferredUsername;
  preferred_match.password_element = u"password";
  preferred_match.password_value = kPreferredPassword;
  preferred_match.submit_element = u"";
  preferred_match.signon_realm = "https://foo.com/";
  preferred_match.match_type = PasswordForm::MatchType::kPSL;
  preferred_match.scheme = PasswordForm::Scheme::kHtml;

  // Create a match that matches exactly.
  PasswordForm exact_match;
  exact_match.url = GURL("https://foo.com/");
  exact_match.action = GURL("https://foo.com/login");
  exact_match.username_element = u"username";
  exact_match.username_value = u"test1@gmail.com";
  exact_match.password_element = u"password";
  exact_match.password_value = kPreferredPassword;
  exact_match.submit_element = u"";
  exact_match.signon_realm = "https://foo.com/";
  exact_match.scheme = PasswordForm::Scheme::kHtml;
  exact_match.match_type = PasswordForm::MatchType::kExact;

  // Create a match that was matched using public suffix.
  PasswordForm public_suffix_match;
  public_suffix_match.url = GURL("https://foo.com/");
  public_suffix_match.action = GURL("https://foo.com/login");
  public_suffix_match.username_element = u"username";
  public_suffix_match.username_value = u"test2@gmail.com";
  public_suffix_match.password_element = u"password";
  public_suffix_match.password_value = kPreferredPassword;
  public_suffix_match.submit_element = u"";
  public_suffix_match.match_type = PasswordForm::MatchType::kPSL;
  public_suffix_match.signon_realm = "https://foo.com/";
  public_suffix_match.scheme = PasswordForm::Scheme::kHtml;

  Origin page_origin = Origin::Create(GURL("https://foo.com/"));

  // Add one exact match and one public suffix match.
  std::vector<PasswordForm> matches = {exact_match, public_suffix_match};

  PasswordFormFillData result = CreatePasswordFormFillData(
      form_on_page, matches, preferred_match, page_origin,
      /*wait_for_username=*/true, /*suggestion_banned_fields=*/{});
  EXPECT_TRUE(result.wait_for_username);
  // The preferred realm should match the signon realm from the
  // preferred match so the user can see where the result came from.
  EXPECT_EQ(preferred_match.signon_realm, result.preferred_login.realm);

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
  form_on_page.username_element = u"username";
  form_on_page.username_value = kPreferredUsername;
  form_on_page.password_element = u"password";
  form_on_page.password_value = kPreferredPassword;
  form_on_page.submit_element = u"";
  form_on_page.signon_realm = "https://foo.com/";
  form_on_page.scheme = PasswordForm::Scheme::kHtml;

  // Create a match from the database that matches using affiliation.
  PasswordForm preferred_match;
  preferred_match.url = GURL("android://hash@foo.com/");
  preferred_match.username_value = kPreferredUsername;
  preferred_match.password_value = kPreferredPassword;
  preferred_match.signon_realm = "android://hash@foo.com/";
  preferred_match.match_type = PasswordForm::MatchType::kAffiliated;

  // Create a match that matches exactly.
  PasswordForm exact_match;
  exact_match.url = GURL("https://foo.com/");
  exact_match.action = GURL("https://foo.com/login");
  exact_match.username_element = u"username";
  exact_match.username_value = u"test1@gmail.com";
  exact_match.password_element = u"password";
  exact_match.password_value = kPreferredPassword;
  exact_match.submit_element = u"";
  exact_match.signon_realm = "https://foo.com/";
  exact_match.scheme = PasswordForm::Scheme::kHtml;
  exact_match.match_type = PasswordForm::MatchType::kExact;

  // Create a match that was matched using public suffix, so
  // |is_public_suffix_match| == true.
  PasswordForm affiliated_match;
  affiliated_match.url = GURL("android://hash@foo1.com/");
  affiliated_match.username_value = u"test2@gmail.com";
  affiliated_match.password_value = kPreferredPassword;
  affiliated_match.signon_realm = "https://foo1.com/";
  affiliated_match.scheme = PasswordForm::Scheme::kHtml;
  affiliated_match.match_type = PasswordForm::MatchType::kAffiliated;

  Origin page_origin = Origin::Create(GURL("https://foo.com/"));

  // Add one exact match and one affiliation based match.
  std::vector<PasswordForm> matches = {exact_match, affiliated_match};

  PasswordFormFillData result = CreatePasswordFormFillData(
      form_on_page, matches, preferred_match, page_origin,
      /*wait_for_username=*/false, /*suggestion_banned_fields=*/{});
  EXPECT_FALSE(result.wait_for_username);
  // The preferred realm should match the signon realm from the
  // preferred match so the user can see where the result came from.
  EXPECT_EQ(preferred_match.signon_realm, result.preferred_login.realm);

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
  form_on_page.username_element = u"username";
  form_on_page.password_element = u"password";
  form_on_page.username_may_use_prefilled_placeholder = true;
  form_on_page.server_side_classification_successful = true;

  // Create an exact match in the database.
  PasswordForm preferred_match = form_on_page;
  preferred_match.username_value = kPreferredUsername;
  preferred_match.password_value = kPreferredPassword;
  preferred_match.match_type = PasswordForm::MatchType::kExact;

  // Set renderer id related fields.
  FormData form_data;
  form_data.set_host_frame(autofill::LocalFrameToken(
      base::UnguessableToken::CreateForTesting(98765, 43210)));
  form_data.set_renderer_id(FormRendererId(42));
  form_on_page.form_data = form_data;
  form_on_page.username_element_renderer_id = FieldRendererId(123);
  form_on_page.password_element_renderer_id = FieldRendererId(456);

  Origin page_origin = Origin::Create(GURL("https://foo.com/"));

  PasswordFormFillData result = CreatePasswordFormFillData(
      form_on_page, {}, preferred_match, page_origin,
      /*wait_for_username=*/true, /*suggestion_banned_fields=*/{});

  EXPECT_EQ(form_data.renderer_id(), result.form_renderer_id);
  EXPECT_EQ(form_on_page.username_element_renderer_id,
            result.username_element_renderer_id);
  EXPECT_EQ(form_on_page.password_element_renderer_id,
            result.password_element_renderer_id);
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
  preferred_match.username_value = kPreferredUsername;
  preferred_match.password_value = kPreferredPassword;
  preferred_match.match_type = PasswordForm::MatchType::kExact;

  FormData form_data;
  form_data.set_renderer_id(FormRendererId(42));
  form_on_page.form_data = form_data;

  Origin page_origin = Origin::Create(GURL("https://foo.com/"));

  PasswordFormFillData result = CreatePasswordFormFillData(
      form_on_page, {} /* matches */, preferred_match, page_origin,
      /*wait_for_username=*/true, /*suggestion_banned_fields=*/{});

  // Check that nor username nor password fields are set.
  EXPECT_TRUE(result.username_element_renderer_id.is_null());
  EXPECT_TRUE(result.password_element_renderer_id.is_null());
}

// Tests that the constructing a PasswordFormFillData behaves correctly when
// there is a preferred match that was found using affiliation based matching,
// with app_display_name.
TEST(PasswordFormFillDataTest, TestAffiliationWithAppName) {
  // Create the current form on the page.
  PasswordForm form_on_page;
  form_on_page.url = GURL("https://foo.com/");
  form_on_page.action = GURL("https://foo.com/login");
  form_on_page.username_element = u"username";
  form_on_page.username_value = kPreferredUsername;
  form_on_page.password_element = u"password";
  form_on_page.password_value = kPreferredPassword;
  form_on_page.signon_realm = "https://foo.com/";
  form_on_page.scheme = PasswordForm::Scheme::kHtml;
  form_on_page.match_type = PasswordForm::MatchType::kExact;

  // Create a match that was matched using affiliation matching.
  PasswordForm affiliated_match;
  affiliated_match.url = GURL("android://hash@foo1.com/");
  affiliated_match.username_value = u"test2@gmail.com";
  affiliated_match.password_value = kPreferredPassword;
  affiliated_match.match_type = PasswordForm::MatchType::kAffiliated;
  affiliated_match.app_display_name = "Foo";
  affiliated_match.signon_realm = "https://foo1.com/";
  affiliated_match.scheme = PasswordForm::Scheme::kHtml;

  Origin page_origin = Origin::Create(GURL("https://foo.com/"));

  // Add one exact match and one affiliation based match.
  std::vector<PasswordForm> matches = {affiliated_match};

  PasswordFormFillData result = CreatePasswordFormFillData(
      form_on_page, matches, affiliated_match, page_origin,
      /*wait_for_username=*/false, /*suggestion_banned_fields=*/{});
  EXPECT_FALSE(result.wait_for_username);
  // The preferred realm should match the app name from the affiliated match so
  // the user can see and understand where the result came from.
  EXPECT_EQ(affiliated_match.app_display_name, result.preferred_login.realm);
}

// Tests that the constructing a PasswordFormFillData behaves correctly inside
// cross-origin iframes.
TEST(PasswordFormFillDataTest, TestCrossOriginIframe) {
  // Create the current form on the page.
  PasswordForm form_on_page;
  form_on_page.url = GURL("https://foo.com/");
  form_on_page.action = GURL("https://foo.com/login");
  form_on_page.username_element = u"username";
  form_on_page.username_value = kPreferredUsername;
  form_on_page.password_element = u"password";
  form_on_page.password_value = kPreferredPassword;
  form_on_page.signon_realm = "https://foo.com/";
  form_on_page.submit_element = u"";
  form_on_page.scheme = PasswordForm::Scheme::kHtml;
  form_on_page.match_type = PasswordForm::MatchType::kExact;

  // Create the current form on the page.
  PasswordForm additional_match = form_on_page;
  additional_match.username_value = u"test2@gmail.com";

  Origin page_origin = Origin::Create(GURL("https://chromium.com/"));

  // Add one exact match and one affiliation based match.
  std::vector<PasswordForm> matches = {additional_match};

  PasswordFormFillData result = CreatePasswordFormFillData(
      form_on_page, matches, form_on_page, page_origin,
      /*wait_for_username=*/false, /*suggestion_banned_fields=*/{});
  EXPECT_FALSE(result.wait_for_username);

  // The preferred realm should match the form signon_realm.
  EXPECT_EQ(result.preferred_login.realm, form_on_page.signon_realm);
  // The realm of the additional login match should match the form
  // signon_realm.
  EXPECT_EQ(result.additional_logins[0].realm, additional_match.signon_realm);
}

}  // namespace password_manager
