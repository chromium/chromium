// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manual_fallback_flow.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/ui/autofill_suggestion_delegate.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_test_helpers.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/password_manager/core/browser/affiliation/mock_affiliated_match_helper.h"
#include "components/password_manager/core/browser/form_parsing/form_data_parser.h"
#include "components/password_manager/core/browser/mock_password_form_cache.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_manual_fallback_metrics_recorder.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

using autofill::AutofillClient;
using autofill::AutofillSuggestionDelegate;
using autofill::AutofillSuggestionTriggerSource;
using autofill::FieldRendererId;
using autofill::Suggestion;
using autofill::SuggestionHidingReason;
using autofill::SuggestionType;
using autofill::TestAutofillClient;
using autofill::test::AutofillUnitTestEnvironment;
using autofill::test::MakeFieldRendererId;
using base::i18n::TextDirection;
using base::test::RunOnceCallback;
using testing::_;
using testing::AllOf;
using testing::ByMove;
using testing::ElementsAre;
using testing::Field;
using testing::Matcher;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;
using testing::Test;
using testing::Values;

constexpr const char kUrl[] = "https://example.com/";
constexpr const char kPSLExtension[] = "https://psl.example.com/";
constexpr const char kUrlWithNoExactMatches[] = "https://www.foo.com/";

constexpr char kShowSuggestionLatency[] =
    "PasswordManager.ManualFallback.ShowSuggestions.Latency";

Matcher<Suggestion> EqualsManualFallbackSuggestion(SuggestionType type,
                                                   bool is_acceptable) {
  return AllOf(
      Field("type", &Suggestion::type, type),
      Field("is_acceptable", &Suggestion::is_acceptable, is_acceptable));
}

Suggestion::PasswordSuggestionDetails CreateTestPasswordDetails() {
  return Suggestion::PasswordSuggestionDetails(
      u"username", u"password", "https://google.com/", u"google.com",
      /*is_cross_domain=*/false);
}

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() = default;
  ~MockAutofillClient() override = default;
  MOCK_METHOD(autofill::AutofillClient::SuggestionUiSessionId,
              ShowAutofillSuggestions,
              (const AutofillClient::PopupOpenArgs&,
               base::WeakPtr<AutofillSuggestionDelegate>),
              (override));
  MOCK_METHOD(void,
              HideAutofillSuggestions,
              (SuggestionHidingReason),
              (override));
};

class MockPasswordManagerDriver : public StubPasswordManagerDriver {
 public:
  MockPasswordManagerDriver() = default;
  ~MockPasswordManagerDriver() override = default;
  MOCK_METHOD(void,
              PreviewField,
              (FieldRendererId, const std::u16string&),
              (override));
  MOCK_METHOD(void,
              PreviewSuggestion,
              (const std::u16string&, const std::u16string&),
              (override));
  MOCK_METHOD(void,
              FillSuggestion,
              (const std::u16string&, const std::u16string&),
              (override));
  MOCK_METHOD(void,
              FillSuggestionById,
              (FieldRendererId,
               FieldRendererId,
               const std::u16string&,
               const std::u16string&),
              (override));
  MOCK_METHOD(void,
              PreviewSuggestionById,
              (FieldRendererId,
               FieldRendererId,
               const std::u16string&,
               const std::u16string&),
              (override));
  MOCK_METHOD(void, FillField, (const std::u16string&), (override));
  MOCK_METHOD(const GURL&, GetLastCommittedURL, (), (const override));
};

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() = default;
  ~MockPasswordManagerClient() override = default;
  MOCK_METHOD(void,
              NavigateToManagePasswordsPage,
              (ManagePasswordsReferrer),
              (override));
  MOCK_METHOD(bool,
              IsReauthBeforeFillingRequired,
              (device_reauth::DeviceAuthenticator*),
              (override));
  MOCK_METHOD(std::unique_ptr<device_reauth::DeviceAuthenticator>,
              GetDeviceAuthenticator,
              (),
              (override));
  MOCK_METHOD(PasswordStoreInterface*,
              GetProfilePasswordStore,
              (),
              (const override));
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_CHROMEOS)
  MOCK_METHOD(void,
              OpenPasswordDetailsBubble,
              (const password_manager::PasswordForm& form),
              (override));
  MOCK_METHOD(std::unique_ptr<PasswordCrossDomainConfirmationPopupController>,
              ShowCrossDomainConfirmationPopup,
              (const gfx::RectF& element_bounds,
               base::i18n::TextDirection text_direction,
               const GURL& domain,
               const std::u16string& password_origin,
               base::OnceClosure confirmation_callback),
              (override));
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_CHROMEOS)
};

class MockAffiliationService : public affiliations::FakeAffiliationService {
 public:
  MockAffiliationService() = default;
  ~MockAffiliationService() override = default;
  MOCK_METHOD(void,
              GetPSLExtensions,
              (base::OnceCallback<void(std::vector<std::string>)>),
              (const override));
};

class PasswordManualFallbackFlowTest : public Test {
 public:
  PasswordManualFallbackFlowTest() {
    ON_CALL(password_manager_client(), GetProfilePasswordStore)
        .WillByDefault(Return(&profile_password_store()));

    auto profile_store_match_helper =
        std::make_unique<NiceMock<MockAffiliatedMatchHelper>>(
            affiliation_service_.get());
    mock_affiliated_match_helper_ = profile_store_match_helper.get();
    profile_password_store().Init(/*prefs=*/nullptr,
                                  std::move(profile_store_match_helper));
  }

  ~PasswordManualFallbackFlowTest() override {
    mock_affiliated_match_helper_ = nullptr;
    profile_password_store_->ShutdownOnUIThread();
  }

  void SetUp() override {
    Test::SetUp();

    // Add 1 password form to the password store.
    profile_password_store().AddLogin(
        CreateEntry("username@example.com", "password", GURL(kUrl),
                    PasswordForm::MatchType::kExact));
  }

  void TearDown() override {
    profile_password_store().Clear();
    Test::TearDown();
  }

  PasswordManualFallbackFlow& flow() { return *flow_; }

  MockPasswordManagerDriver& driver() { return *driver_; }

  MockAutofillClient& autofill_client() { return *autofill_client_; }

  MockPasswordManagerClient& password_manager_client() {
    return *password_manager_client_;
  }

  MockPasswordFormCache& password_form_cache() { return password_form_cache_; }

  MockAffiliationService& affiliation_service() {
    return *affiliation_service_;
  }

  MockAffiliatedMatchHelper& affiliated_match_helper() {
    return *mock_affiliated_match_helper_;
  }

  TestPasswordStore& profile_password_store() {
    return *profile_password_store_;
  }

  void InitializeFlow(const std::string& form_domain = kUrl,
                      const std::vector<std::string>& psl_extensions = {}) {
    triggering_form_domain_ = GURL(form_domain);
    ON_CALL(driver(), GetLastCommittedURL)
        .WillByDefault(ReturnRef(triggering_form_domain_));
    // `GetPSLExtensions` must be defined so that password store requests always
    // complete.
    ON_CALL(affiliation_service(), GetPSLExtensions)
        .WillByDefault(
            [psl_extensions](
                base::OnceCallback<void(std::vector<std::string>)> callback) {
              std::move(callback).Run(psl_extensions);
            });

    std::unique_ptr<SavedPasswordsPresenter> passwords_presenter =
        std::make_unique<SavedPasswordsPresenter>(
            &affiliation_service(), profile_password_store_,
            /*account_password_store_=*/nullptr);

    manual_fallback_metrics_recorder_ =
        std::make_unique<PasswordManualFallbackMetricsRecorder>();

    flow_ = std::make_unique<PasswordManualFallbackFlow>(
        &driver(), &autofill_client(), &password_manager_client(),
        manual_fallback_metrics_recorder_.get(), &password_form_cache(),
        std::move(passwords_presenter));
  }

  void ShowAndAcceptSuggestion(
      const Suggestion& suggestion,
      const autofill::AutofillSuggestionDelegate::SuggestionMetadata&
          metadata) {
    // In production, suggestions cannot be accepted if not shown first.
    // Simulating showing them in tests is mandatory, otherwise a `CHECK` error
    // would occur while logging metrics.
    flow().OnSuggestionsShown(base::span_from_ref(suggestion));
    flow().DidAcceptSuggestion(suggestion, metadata);
  }

  // The test fixture relies on the fact that `TestPasswordStore` performs all
  // operation asynchronously.
  void ProcessPasswordStoreUpdates() { task_environment_.RunUntilIdle(); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  AutofillUnitTestEnvironment autofill_test_environment_;
  std::unique_ptr<NiceMock<MockPasswordManagerDriver>> driver_ =
      std::make_unique<NiceMock<MockPasswordManagerDriver>>();
  std::unique_ptr<NiceMock<MockAutofillClient>> autofill_client_ =
      std::make_unique<NiceMock<MockAutofillClient>>();
  std::unique_ptr<NiceMock<MockPasswordManagerClient>>
      password_manager_client_ =
          std::make_unique<NiceMock<MockPasswordManagerClient>>();
  NiceMock<MockPasswordFormCache> password_form_cache_;
  std::unique_ptr<PasswordManualFallbackMetricsRecorder>
      manual_fallback_metrics_recorder_;
  std::unique_ptr<NiceMock<MockAffiliationService>> affiliation_service_ =
      std::make_unique<NiceMock<MockAffiliationService>>();
  raw_ptr<MockAffiliatedMatchHelper> mock_affiliated_match_helper_;
  scoped_refptr<TestPasswordStore> profile_password_store_ =
      base::MakeRefCounted<TestPasswordStore>();
  GURL triggering_form_domain_ = GURL::EmptyGURL();
  std::unique_ptr<PasswordManualFallbackFlow> flow_;
};

// Test that no suggestions are shown before the passwords are read from disk.
TEST_F(PasswordManualFallbackFlowTest, RunFlow_NoSuggestionsReturned) {
  base::HistogramTester histogram_tester;
  InitializeFlow();

  EXPECT_CALL(autofill_client(), ShowAutofillSuggestions).Times(0);

  flow().RunFlow(MakeFieldRendererId(), gfx::RectF{},
                 TextDirection::LEFT_TO_RIGHT);
  // Latency should not be logged if the passwords are not read from disk.
  histogram_tester.ExpectTotalCount(kShowSuggestionLatency, 0);
}

// Test that the suggestions are not shown when the passwords are fetched from
// disk.
TEST_F(PasswordManualFallbackFlowTest, ReturnSuggestions_NoFlowInvocation) {
  base::HistogramTester histogram_tester;
  InitializeFlow();

  EXPECT_CALL(autofill_client(), ShowAutofillSuggestions).Times(0);

  ProcessPasswordStoreUpdates();
  // The latency should be logged if the passwords are read from disk but the
  // flow is not invoked.
  histogram_tester.ExpectTotalCount(kShowSuggestionLatency, 1);
}

// Test that the suggestions are shown when the flow is invoked after the
// suggestions were read from disk.
TEST_F(PasswordManualFallbackFlowTest, ReturnSuggestions_InvokeFlow) {
  base::HistogramTester histogram_tester;
  InitializeFlow();
  ProcessPasswordStoreUpdates();

  const gfx::RectF bounds(1, 1, 2, 2);
  EXPECT_CALL(
      autofill_client(),
      ShowAutofillSuggestions(
          AllOf(
              Field("element_bounds",
                    &AutofillClient::PopupOpenArgs::element_bounds, bounds),
              Field("text_direction",
                    &AutofillClient::PopupOpenArgs::text_direction,
                    TextDirection::LEFT_TO_RIGHT),
              Field("trigger_source",
                    &AutofillClient::PopupOpenArgs::trigger_source,
                    AutofillSuggestionTriggerSource::kManualFallbackPasswords)),
          _));

  flow().RunFlow(MakeFieldRendererId(), bounds, TextDirection::LEFT_TO_RIGHT);
  // The latency should be logged if the passwords are read from disk before the
  // flow is invoked.
  histogram_tester.ExpectTotalCount(kShowSuggestionLatency, 1);
}

// Test that the suggestions are shown when the flow is invoked before the
// suggestions were read from disk.
TEST_F(PasswordManualFallbackFlowTest, InvokeFlow_ReturnSuggestions) {
  base::HistogramTester histogram_tester;
  InitializeFlow();

  const gfx::RectF bounds(1, 1, 2, 2);
  flow().RunFlow(MakeFieldRendererId(), bounds, TextDirection::LEFT_TO_RIGHT);

  EXPECT_CALL(
      autofill_client(),
      ShowAutofillSuggestions(
          AllOf(
              Field("element_bounds",
                    &AutofillClient::PopupOpenArgs::element_bounds, bounds),
              Field("text_direction",
                    &AutofillClient::PopupOpenArgs::text_direction,
                    TextDirection::LEFT_TO_RIGHT),
              Field("trigger_source",
                    &AutofillClient::PopupOpenArgs::trigger_source,
                    AutofillSuggestionTriggerSource::kManualFallbackPasswords)),
          _));

  ProcessPasswordStoreUpdates();
  // The latency should be logged if the passwords are read from disk after the
  // flow is invoked.
  histogram_tester.ExpectTotalCount(kShowSuggestionLatency, 1);
}

// Test that the suggestions are shown using the last parameters passed to
// `RunFlow` when the suggestions are read from disk.
TEST_F(PasswordManualFallbackFlowTest, LastRunParametersAreUsed) {
  InitializeFlow();

  const gfx::RectF bounds_1(1, 1, 2, 2);
  const gfx::RectF bounds_2(2, 2, 4, 4);
  flow().RunFlow(MakeFieldRendererId(), bounds_1, TextDirection::LEFT_TO_RIGHT);
  flow().RunFlow(MakeFieldRendererId(), bounds_2, TextDirection::RIGHT_TO_LEFT);

  EXPECT_CALL(
      autofill_client(),
      ShowAutofillSuggestions(
          AllOf(
              Field("element_bounds",
                    &AutofillClient::PopupOpenArgs::element_bounds, bounds_2),
              Field("text_direction",
                    &AutofillClient::PopupOpenArgs::text_direction,
                    TextDirection::RIGHT_TO_LEFT),
              Field("trigger_source",
                    &AutofillClient::PopupOpenArgs::trigger_source,
                    AutofillSuggestionTriggerSource::kManualFallbackPasswords)),
          _));
  ProcessPasswordStoreUpdates();
}

// Manual fallback flow can be invoked multiple times. This test verifies that
// the Autofill popup is opened multiple times in this case given that the
// passwords were retrieved from disk.
TEST_F(PasswordManualFallbackFlowTest, RunFlowMultipleTimes) {
  InitializeFlow();
  ProcessPasswordStoreUpdates();

  const gfx::RectF bounds_1(1, 1, 2, 2);
  const gfx::RectF bounds_2(2, 2, 4, 4);
  EXPECT_CALL(
      autofill_client(),
      ShowAutofillSuggestions(
          AllOf(
              Field("element_bounds",
                    &AutofillClient::PopupOpenArgs::element_bounds, bounds_1),
              Field("text_direction",
                    &AutofillClient::PopupOpenArgs::text_direction,
                    TextDirection::LEFT_TO_RIGHT),
              Field("trigger_source",
                    &AutofillClient::PopupOpenArgs::trigger_source,
                    AutofillSuggestionTriggerSource::kManualFallbackPasswords)),
          _));
  flow().RunFlow(MakeFieldRendererId(), bounds_1, TextDirection::LEFT_TO_RIGHT);

  EXPECT_CALL(
      autofill_client(),
      ShowAutofillSuggestions(
          AllOf(
              Field("element_bounds",
                    &AutofillClient::PopupOpenArgs::element_bounds, bounds_2),
              Field("text_direction",
                    &AutofillClient::PopupOpenArgs::text_direction,
                    TextDirection::RIGHT_TO_LEFT),
              Field("trigger_source",
                    &AutofillClient::PopupOpenArgs::trigger_source,
                    AutofillSuggestionTriggerSource::kManualFallbackPasswords)),
          _));
  flow().RunFlow(MakeFieldRendererId(), bounds_2, TextDirection::RIGHT_TO_LEFT);
}

// Manual fallback should not show passwords from different domains in the
// "Suggested" section.
TEST_F(PasswordManualFallbackFlowTest, DifferentDomain_NoSuggestedPasswords) {
  InitializeFlow(kUrlWithNoExactMatches);
  ProcessPasswordStoreUpdates();

  const gfx::RectF bounds(1, 1, 2, 2);
  EXPECT_CALL(
      autofill_client(),
      ShowAutofillSuggestions(
          AllOf(
              Field("element_bounds",
                    &AutofillClient::PopupOpenArgs::element_bounds, bounds),
              Field("text_direction",
                    &AutofillClient::PopupOpenArgs::text_direction,
                    TextDirection::LEFT_TO_RIGHT),
              Field("trigger_source",
                    &AutofillClient::PopupOpenArgs::trigger_source,
                    AutofillSuggestionTriggerSource::kManualFallbackPasswords),
              Field(
                  "suggestions", &AutofillClient::PopupOpenArgs::suggestions,
                  ElementsAre(EqualsSuggestion(SuggestionType::kPasswordEntry),
                              EqualsSuggestion(SuggestionType::kSeparator),
                              EqualsSuggestion(
                                  SuggestionType::kAllSavedPasswordsEntry)))),
          _));
  flow().RunFlow(MakeFieldRendererId(), bounds, TextDirection::LEFT_TO_RIGHT);
}

// Manual fallback should show PSL matched passwords in the "Suggested" section.
TEST_F(PasswordManualFallbackFlowTest,
       DifferentDomain_SuggestsPlsMatchedPasswords) {
  // Register `kUrl` as the PSL extension of the `kPSLExtension`.
  InitializeFlow(kPSLExtension, {kUrl});
  ProcessPasswordStoreUpdates();

  const gfx::RectF bounds(1, 1, 2, 2);
  EXPECT_CALL(
      autofill_client(),
      ShowAutofillSuggestions(
          AllOf(
              Field("element_bounds",
                    &AutofillClient::PopupOpenArgs::element_bounds, bounds),
              Field("text_direction",
                    &AutofillClient::PopupOpenArgs::text_direction,
                    TextDirection::LEFT_TO_RIGHT),
              Field("trigger_source",
                    &AutofillClient::PopupOpenArgs::trigger_source,
                    AutofillSuggestionTriggerSource::kManualFallbackPasswords),
              Field(
                  "suggestions", &AutofillClient::PopupOpenArgs::suggestions,
                  ElementsAre(EqualsSuggestion(SuggestionType::kTitle),
                              EqualsSuggestion(SuggestionType::kPasswordEntry),
                              EqualsSuggestion(SuggestionType::kTitle),
                              EqualsSuggestion(SuggestionType::kPasswordEntry),
                              EqualsSuggestion(SuggestionType::kSeparator),
                              EqualsSuggestion(
                                  SuggestionType::kAllSavedPasswordsEntry)))),
          _));
  flow().RunFlow(MakeFieldRendererId(), bounds, TextDirection::LEFT_TO_RIGHT);
}

// Manual fallback should show grouped passwords from different domains in the
// "Suggested" section.
TEST_F(PasswordManualFallbackFlowTest,
       DifferentDomain_NoGroupedPasswordsShown) {
  // Register `kUrl` domain as weakly affiliated with the
  // `kUrlWithNoExactMatches` domain.
  PasswordFormDigest digest(PasswordForm::Scheme::kHtml,
                            GetSignonRealm(GURL(kUrlWithNoExactMatches)),
                            GURL(kUrlWithNoExactMatches));
  affiliated_match_helper().ExpectCallToGetAffiliatedAndGrouped(
      digest, {kUrlWithNoExactMatches}, {kUrl});
  // Trigger flow for the `kUrlWithNoExactMatches` domain.
  InitializeFlow(kUrlWithNoExactMatches);
  ProcessPasswordStoreUpdates();

  const gfx::RectF bounds(1, 1, 2, 2);
  // Expect that grouped credentials are not shown in the "Suggested" passwords
  // section.
  EXPECT_CALL(
      autofill_client(),
      ShowAutofillSuggestions(
          AllOf(
              Field("element_bounds",
                    &AutofillClient::PopupOpenArgs::element_bounds, bounds),
              Field("text_direction",
                    &AutofillClient::PopupOpenArgs::text_direction,
                    TextDirection::LEFT_TO_RIGHT),
              Field("trigger_source",
                    &AutofillClient::PopupOpenArgs::trigger_source,
                    AutofillSuggestionTriggerSource::kManualFallbackPasswords),
              Field(
                  "suggestions", &AutofillClient::PopupOpenArgs::suggestions,
                  ElementsAre(EqualsSuggestion(SuggestionType::kTitle),
                              EqualsSuggestion(SuggestionType::kPasswordEntry),
                              EqualsSuggestion(SuggestionType::kTitle),
                              EqualsSuggestion(SuggestionType::kPasswordEntry),
                              EqualsSuggestion(SuggestionType::kSeparator),
                              EqualsSuggestion(
                                  SuggestionType::kAllSavedPasswordsEntry)))),
          _));
  flow().RunFlow(MakeFieldRendererId(), bounds, TextDirection::LEFT_TO_RIGHT);
}

// Manual fallback should show exact domain matches in the "Suggested" section.
TEST_F(PasswordManualFallbackFlowTest, SameDomain_SuggestsExactMatches) {
  InitializeFlow();
  ProcessPasswordStoreUpdates();

  const gfx::RectF bounds(1, 1, 2, 2);
  EXPECT_CALL(
      autofill_client(),
      ShowAutofillSuggestions(
          AllOf(
              Field("element_bounds",
                    &AutofillClient::PopupOpenArgs::element_bounds, bounds),
              Field("text_direction",
                    &AutofillClient::PopupOpenArgs::text_direction,
                    TextDirection::LEFT_TO_RIGHT),
              Field("trigger_source",
                    &AutofillClient::PopupOpenArgs::trigger_source,
                    AutofillSuggestionTriggerSource::kManualFallbackPasswords),
              Field(
                  "suggestions", &AutofillClient::PopupOpenArgs::suggestions,
                  ElementsAre(EqualsSuggestion(SuggestionType::kTitle),
                              EqualsSuggestion(SuggestionType::kPasswordEntry),
                              EqualsSuggestion(SuggestionType::kTitle),
                              EqualsSuggestion(SuggestionType::kPasswordEntry),
                              EqualsSuggestion(SuggestionType::kSeparator),
                              EqualsSuggestion(
                                  SuggestionType::kAllSavedPasswordsEntry)))),
          _));
  flow().RunFlow(MakeFieldRendererId(), bounds, TextDirection::LEFT_TO_RIGHT);
}

// Test that username field-by-field suggestion is previewed into the correct
// field by the manual fallback flow.
TEST_F(PasswordManualFallbackFlowTest, SelectUsernameFieldByFieldSuggestion) {
  InitializeFlow();
  ProcessPasswordStoreUpdates();

  const FieldRendererId field_id = MakeFieldRendererId();
  flow().RunFlow(field_id, gfx::RectF{}, TextDirection::LEFT_TO_RIGHT);

  EXPECT_CALL(driver(),
              PreviewField(field_id, std::u16string(u"username@example.com")));
  flow().DidSelectSuggestion(autofill::test::CreateAutofillSuggestion(
      SuggestionType::kPasswordFieldByFieldFilling, u"username@example.com"));
}

// Test that username field-by-field suggestion is filled into the correct field
// by the manual fallback flow.
TEST_F(PasswordManualFallbackFlowTest, AcceptUsernameFieldByFieldSuggestion) {
  InitializeFlow();
  ProcessPasswordStoreUpdates();

  const FieldRendererId field_id = MakeFieldRendererId();
  flow().RunFlow(field_id, gfx::RectF{}, TextDirection::LEFT_TO_RIGHT);

  EXPECT_CALL(driver(), FillField(std::u16string(u"username@example.com")));
  EXPECT_CALL(
      autofill_client(),
      HideAutofillSuggestions(SuggestionHidingReason::kAcceptSuggestion));
  ShowAndAcceptSuggestion(autofill::test::CreateAutofillSuggestion(
                              SuggestionType::kPasswordFieldByFieldFilling,
                              u"username@example.com"),
                          AutofillSuggestionDelegate::SuggestionMetadata{
                              .row = 0, .sub_popup_level = 1});
}

// Test that both username and password are previewed if the suggestion is
// selected for a popup triggered on a password form.
TEST_F(PasswordManualFallbackFlowTest,
       SelectFillFullFormSuggestion_TriggeredOnAPasswordForm) {
  InitializeFlow();
  ProcessPasswordStoreUpdates();

  PasswordForm form;
  form.username_element_renderer_id = MakeFieldRendererId();
  form.password_element_renderer_id = MakeFieldRendererId();
  // Simulate that the field is/isn't classified as target filling password.
  EXPECT_CALL(password_form_cache(),
              GetPasswordForm(_, form.username_element_renderer_id))
      .WillRepeatedly(Return(&form));

  flow().RunFlow(form.username_element_renderer_id, gfx::RectF{},
                 TextDirection::LEFT_TO_RIGHT);

  EXPECT_CALL(driver(), PreviewSuggestionById(form.username_element_renderer_id,
                                              form.password_element_renderer_id,
                                              std::u16string(u"username"),
                                              std::u16string(u"password")));
  Suggestion suggestion = autofill::test::CreateAutofillSuggestion(
      SuggestionType::kPasswordEntry, u"google.com",
      CreateTestPasswordDetails());
  suggestion.labels = {{Suggestion::Text(u"username")}};
  // `suggestion.is_acceptable` is `true` if the popup is triggered on a
  // password form.
  suggestion.is_acceptable = true;
  flow().DidSelectSuggestion(suggestion);
}

// Test that only password field is previewed if the credential doesn't have
// a username saved for it.
TEST_F(PasswordManualFallbackFlowTest,
       SelectFillFullFormSuggestion_NoUsername_TriggeredOnAPasswordForm) {
  InitializeFlow();
  ProcessPasswordStoreUpdates();

  PasswordForm form;
  form.password_element_renderer_id = MakeFieldRendererId();
  // Simulate that the field is/isn't classified as target filling password.
  EXPECT_CALL(password_form_cache(),
              GetPasswordForm(_, form.password_element_renderer_id))
      .WillRepeatedly(Return(&form));

  flow().RunFlow(form.password_element_renderer_id, gfx::RectF{},
                 TextDirection::LEFT_TO_RIGHT);

  EXPECT_CALL(driver(), PreviewSuggestionById(FieldRendererId(),
                                              form.password_element_renderer_id,
                                              std::u16string(),
                                              std::u16string(u"password")));
  Suggestion suggestion = autofill::test::CreateAutofillSuggestion(
      SuggestionType::kPasswordEntry, u"google.com",
      CreateTestPasswordDetails());
  suggestion.labels = {{Suggestion::Text(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN))}};
  // `suggestion.is_acceptable` is `true` if the popup is triggered on a
  // password form.
  suggestion.is_acceptable = true;
  flow().DidSelectSuggestion(suggestion);
}

// Test that password manual fallback suggestion is not previewed if the popup
// is triggered on a non-password form.
TEST_F(PasswordManualFallbackFlowTest,
       SelectFillFullFormSuggestion_TriggeredOnADifferentForm) {
  InitializeFlow();
  ProcessPasswordStoreUpdates();

  flow().RunFlow(MakeFieldRendererId(), gfx::RectF{},
                 TextDirection::LEFT_TO_RIGHT);

  EXPECT_CALL(driver(), PreviewSuggestion).Times(0);
  Suggestion suggestion = autofill::test::CreateAutofillSuggestion(
      SuggestionType::kPasswordEntry, u"google.com",
      CreateTestPasswordDetails());
  suggestion.labels = {{Suggestion::Text(u"username")}};
  // `suggestion.is_acceptable` is `false` if the popup is triggered on a
  // different type of form or a standalone field.
  suggestion.is_acceptable = false;
  flow().DidSelectSuggestion(suggestion);
}

// Test that both username and password are filled if the suggestion is accepted
// for a popup triggered on a password form if the biometric authentication is
// not available.
TEST_F(PasswordManualFallbackFlowTest,
       AcceptFillFullFormSuggestion_FillsCredentialsIfAuthNotAvailable) {
  InitializeFlow();
  ProcessPasswordStoreUpdates();

  PasswordForm form;
  form.username_element_renderer_id = MakeFieldRendererId();
  form.password_element_renderer_id = MakeFieldRendererId();
  // Simulate that the field is/isn't classified as target filling password.
  EXPECT_CALL(password_form_cache(),
              GetPasswordForm(_, form.username_element_renderer_id))
      .WillRepeatedly(Return(&form));

  flow().RunFlow(form.username_element_renderer_id, gfx::RectF{},
                 TextDirection::LEFT_TO_RIGHT);

  EXPECT_CALL(password_manager_client(), IsReauthBeforeFillingRequired)
      .WillOnce(Return(false));
  EXPECT_CALL(driver(), FillSuggestionById(form.username_element_renderer_id,
                                           form.password_element_renderer_id,
                                           std::u16string(u"username"),
                                           std::u16string(u"password")));
  Suggestion suggestion = autofill::test::CreateAutofillSuggestion(
      SuggestionType::kPasswordEntry, u"google.com",
      CreateTestPasswordDetails());
  suggestion.labels = {{Suggestion::Text(u"username")}};
  // `suggestion.is_acceptable` is `true` if the popup is triggered on a
  // password form.
  suggestion.is_acceptable = true;
  ShowAndAcceptSuggestion(suggestion,
                          AutofillSuggestionDelegate::SuggestionMetadata{
                              .row = 0, .sub_popup_level = 0});
}

// Tests that no credentials are filled if the authentication fails. The popup
// is triggered on a password form.
TEST_F(PasswordManualFallbackFlowTest,
       AcceptFillFullFormSuggestion_NoFillingIfAuthFails) {
  InitializeFlow();
  ProcessPasswordStoreUpdates();

  PasswordForm form;
  form.username_element_renderer_id = MakeFieldRendererId();
  form.password_element_renderer_id = MakeFieldRendererId();
  // Simulate that the field is/isn't classified as target filling password.
  EXPECT_CALL(password_form_cache(),
              GetPasswordForm(_, form.username_element_renderer_id))
      .WillRepeatedly(Return(&form));

  flow().RunFlow(form.username_element_renderer_id, gfx::RectF{},
                 TextDirection::LEFT_TO_RIGHT);

  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  EXPECT_CALL(*authenticator, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(/*auth_succeeded=*/false));

  EXPECT_CALL(password_manager_client(), IsReauthBeforeFillingRequired)
      .WillOnce(Return(true));
  EXPECT_CALL(password_manager_client(), GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(authenticator))));

  EXPECT_CALL(driver(), FillSuggestionById).Times(0);

  base::HistogramTester histograms;
  base::ScopedMockElapsedTimersForTest mock_elapsed_timers_;
  Suggestion suggestion = autofill::test::CreateAutofillSuggestion(
      SuggestionType::kPasswordEntry, u"google.com",
      CreateTestPasswordDetails());
  suggestion.labels = {{Suggestion::Text(u"username")}};
  // `suggestion.is_acceptable` is `true` if the popup is triggered on a
  // password form.
  suggestion.is_acceptable = true;
  ShowAndAcceptSuggestion(suggestion,
                          AutofillSuggestionDelegate::SuggestionMetadata{
                              .row = 0, .sub_popup_level = 0});
  const int64_t kMockElapsedTime =
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime.InMilliseconds();
  histograms.ExpectUniqueSample(
      "PasswordManager.PasswordFilling.AuthenticationResult", false, 1);
  histograms.ExpectUniqueSample(
      "PasswordManager.PasswordFilling.AuthenticationTime2", kMockElapsedTime,
      1);
}

// Tests that credentials are filled if the authentication succeeds. The popup
// is triggered on a password form.
TEST_F(PasswordManualFallbackFlowTest,
       AcceptFillFullFormSuggestion_FillsCredentialsIfAuthSucceeds) {
  InitializeFlow();
  ProcessPasswordStoreUpdates();

  PasswordForm form;
  form.username_element_renderer_id = MakeFieldRendererId();
  form.password_element_renderer_id = MakeFieldRendererId();
  // Simulate that the field is/isn't classified as target filling password.
  EXPECT_CALL(password_form_cache(),
              GetPasswordForm(_, form.username_element_renderer_id))
      .WillRepeatedly(Return(&form));

  flow().RunFlow(form.username_element_renderer_id, gfx::RectF{},
                 TextDirection::LEFT_TO_RIGHT);

  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  EXPECT_CALL(*authenticator, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(/*auth_succeeded=*/true));

  EXPECT_CALL(password_manager_client(), IsReauthBeforeFillingRequired)
      .WillOnce(Return(true));
  EXPECT_CALL(password_manager_client(), GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(authenticator))));

  EXPECT_CALL(driver(), FillSuggestionById(form.username_element_renderer_id,
                                           form.password_element_renderer_id,
                                           std::u16string(u"username"),
                                           std::u16string(u"password")));

  base::HistogramTester histograms;
  base::ScopedMockElapsedTimersForTest mock_elapsed_timers_;
  Suggestion suggestion = autofill::test::CreateAutofillSuggestion(
      SuggestionType::kPasswordEntry, u"google.com",
      CreateTestPasswordDetails());
  suggestion.labels = {{Suggestion::Text(u"username")}};
  // `suggestion.is_acceptable` is `true` if the popup is triggered on a
  // password form.
  suggestion.is_acceptable = true;
  ShowAndAcceptSuggestion(suggestion,
                          AutofillSuggestionDelegate::SuggestionMetadata{
                              .row = 0, .sub_popup_level = 0});
  const int64_t kMockElapsedTime =
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime.InMilliseconds();
  histograms.ExpectUniqueSample(
      "PasswordManager.PasswordFilling.AuthenticationResult", true, 1);
  histograms.ExpectUniqueSample(
      "PasswordManager.PasswordFilling.AuthenticationTime2", kMockElapsedTime,
      1);
}

// Test that only the password is filled if the credential doesn't have the
// username saved for a popup triggered on a password form.
TEST_F(PasswordManualFallbackFlowTest,
       AcceptFillFullFormSuggestion_NoUsername_TriggeredOnAPasswordForm) {
  InitializeFlow();
  ProcessPasswordStoreUpdates();

  PasswordForm form;
  form.password_element_renderer_id = MakeFieldRendererId();
  // Simulate that the field is/isn't classified as target filling password.
  EXPECT_CALL(password_form_cache(),
              GetPasswordForm(_, form.password_element_renderer_id))
      .WillRepeatedly(Return(&form));

  flow().RunFlow(form.password_element_renderer_id, gfx::RectF{},
                 TextDirection::LEFT_TO_RIGHT);

  EXPECT_CALL(
      driver(),
      FillSuggestionById(FieldRendererId(), form.password_element_renderer_id,
                         std::u16string(), std::u16string(u"password")));
  Suggestion suggestion = autofill::test::CreateAutofillSuggestion(
      SuggestionType::kPasswordEntry, u"google.com",
      CreateTestPasswordDetails());
  suggestion.labels = {{Suggestion::Text(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN))}};
  // `suggestion.is_acceptable` is `true` if the popup is triggered on a
  // password form.
  suggestion.is_acceptable = true;
  ShowAndAcceptSuggestion(suggestion,
                          AutofillSuggestionDelegate::SuggestionMetadata{
                              .row = 0, .sub_popup_level = 0});
}

// Test that the password suggestion is not filled if the popup is triggered
// on a non-password form.
TEST_F(PasswordManualFallbackFlowTest,
       AcceptFillFullFormSuggestion_TriggeredOnADifferentForm) {
  InitializeFlow();
  ProcessPasswordStoreUpdates();

  flow().RunFlow(MakeFieldRendererId(), gfx::RectF{},
                 TextDirection::LEFT_TO_RIGHT);

  EXPECT_CALL(driver(), FillSuggestionById).Times(0);
  Suggestion suggestion = autofill::test::CreateAutofillSuggestion(
      SuggestionType::kPasswordEntry, u"google.com",
      CreateTestPasswordDetails());
  suggestion.labels = {{Suggestion::Text(u"username")}};
  // `suggestion.is_acceptable` is `false` if the popup is triggered on a
  // different type of form or a standalone field.
  suggestion.is_acceptable = false;
  ShowAndAcceptSuggestion(suggestion,
                          AutofillSuggestionDelegate::SuggestionMetadata{
                              .row = 0, .sub_popup_level = 0});
}

// Test that "Fill password" field-by-field suggestion is not previewed by the
// manual fallback flow.
TEST_F(PasswordManualFallbackFlowTest,
       SelectFillPasswordFieldByFieldSuggestion) {
  InitializeFlow();
  ProcessPasswordStoreUpdates();

  flow().RunFlow(MakeFieldRendererId(), gfx::RectF{},
                 TextDirection::LEFT_TO_RIGHT);

  EXPECT_CALL(driver(), PreviewField).Times(0);
  flow().DidSelectSuggestion(autofill::test::CreateAutofillSuggestion(
      SuggestionType::kFillPassword, u"Fill password",
      CreateTestPasswordDetails()));
}

// Tests that the password value is filled if the authentication is not
// available. This can happen if it's not implemented for a particular platform.
TEST_F(PasswordManualFallbackFlowTest, FillsPasswordIfAuthNotAvailable) {
  InitializeFlow();
  ProcessPasswordStoreUpdates();

  FieldRendererId field_id = MakeFieldRendererId();
  flow().RunFlow(field_id, gfx::RectF{}, TextDirection::LEFT_TO_RIGHT);

  EXPECT_CALL(password_manager_client(), IsReauthBeforeFillingRequired)
      .WillOnce(Return(false));
  EXPECT_CALL(driver(), FillField(std::u16string(u"password")));
  ShowAndAcceptSuggestion(autofill::test::CreateAutofillSuggestion(
                              SuggestionType::kFillPassword, u"Fill password",
                              CreateTestPasswordDetails()),
                          AutofillSuggestionDelegate::SuggestionMetadata{
                              .row = 0, .sub_popup_level = 1});
}

// Tests that password value if not filled if the authentication fails.
TEST_F(PasswordManualFallbackFlowTest, NoFillingIfAuthFails) {
  InitializeFlow();
  ProcessPasswordStoreUpdates();

  flow().RunFlow(MakeFieldRendererId(), gfx::RectF{},
                 TextDirection::LEFT_TO_RIGHT);

  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  EXPECT_CALL(*authenticator, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(/*auth_succeeded=*/false));

  EXPECT_CALL(password_manager_client(), IsReauthBeforeFillingRequired)
      .WillOnce(Return(true));
  EXPECT_CALL(password_manager_client(), GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(authenticator))));

  EXPECT_CALL(driver(), FillField).Times(0);
  base::HistogramTester histograms;
  base::ScopedMockElapsedTimersForTest mock_elapsed_timers_;
  ShowAndAcceptSuggestion(autofill::test::CreateAutofillSuggestion(
                              SuggestionType::kFillPassword, u"Fill password",
                              CreateTestPasswordDetails()),
                          AutofillSuggestionDelegate::SuggestionMetadata{
                              .row = 0, .sub_popup_level = 1});
  const int64_t kMockElapsedTime =
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime.InMilliseconds();
  histograms.ExpectUniqueSample(
      "PasswordManager.PasswordFilling.AuthenticationResult", false, 1);
  histograms.ExpectUniqueSample(
      "PasswordManager.PasswordFilling.AuthenticationTime2", kMockElapsedTime,
      1);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_CHROMEOS)
class PasswordManualFallbackFlowCrossDomainConfirmationTest
    : public PasswordManualFallbackFlowTest,
      public testing::WithParamInterface<SuggestionType> {};

// Tests that the confirmation popup is shown for cross domain filling.
TEST_P(PasswordManualFallbackFlowCrossDomainConfirmationTest,
       CrossDomainConfirmationBlocksFilling) {
  InitializeFlow();
  ProcessPasswordStoreUpdates();

  const gfx::RectF element_bounds{10, 10, 100, 100};
  const auto text_direction = base::i18n::TextDirection::LEFT_TO_RIGHT;
  const GURL domain = driver().GetLastCommittedURL();
  const std::string password_origin = "password_origin";

  PasswordForm form;
  form.username_element_renderer_id = MakeFieldRendererId();
  form.password_element_renderer_id = MakeFieldRendererId();
  // Simulate that the field is/isn't classified as target filling password.
  EXPECT_CALL(password_form_cache(),
              GetPasswordForm(_, form.username_element_renderer_id))
      .WillRepeatedly(Return(&form));

  flow().RunFlow(form.username_element_renderer_id, element_bounds,
                 text_direction);

  EXPECT_CALL(
      password_manager_client(),
      ShowCrossDomainConfirmationPopup(element_bounds, text_direction, domain,
                                       base::UTF8ToUTF16(password_origin), _));
  EXPECT_CALL(driver(), FillField).Times(0);

  Suggestion suggestion =
      Suggestion(/*main_text=*/"Password", "label", Suggestion::Icon::kKey,
                 /*type=*/GetParam());
  suggestion.payload = Suggestion::PasswordSuggestionDetails(
      u"username", u"password", password_origin,
      base::UTF8ToUTF16(password_origin),
      /*is_cross_domain=*/true);

  ShowAndAcceptSuggestion(std::move(suggestion),
                          AutofillSuggestionDelegate::SuggestionMetadata{});
}

INSTANTIATE_TEST_SUITE_P(PasswordManualFallbackFlowTest,
                         PasswordManualFallbackFlowCrossDomainConfirmationTest,
                         Values(SuggestionType::kPasswordEntry,
                                SuggestionType::kFillPassword));

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_CHROMEOS)

// Tests that password value is filled if the authentication succeeds.
TEST_F(PasswordManualFallbackFlowTest, FillsPasswordIfAuthSucceeds) {
  InitializeFlow();
  ProcessPasswordStoreUpdates();

  FieldRendererId field_id = MakeFieldRendererId();
  flow().RunFlow(field_id, gfx::RectF{}, TextDirection::LEFT_TO_RIGHT);

  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  EXPECT_CALL(*authenticator, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(/*auth_succeeded=*/true));

  EXPECT_CALL(password_manager_client(), IsReauthBeforeFillingRequired)
      .WillOnce(Return(true));
  EXPECT_CALL(password_manager_client(), GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(authenticator))));

  EXPECT_CALL(driver(), FillField(std::u16string(u"password")));
  base::HistogramTester histograms;
  base::ScopedMockElapsedTimersForTest mock_elapsed_timers_;
  ShowAndAcceptSuggestion(autofill::test::CreateAutofillSuggestion(
                              SuggestionType::kFillPassword, u"Fill password",
                              CreateTestPasswordDetails()),
                          AutofillSuggestionDelegate::SuggestionMetadata{
                              .row = 0, .sub_popup_level = 1});
  const int64_t kMockElapsedTime =
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime.InMilliseconds();
  histograms.ExpectUniqueSample(
      "PasswordManager.PasswordFilling.AuthenticationResult", true, 1);
  histograms.ExpectUniqueSample(
      "PasswordManager.PasswordFilling.AuthenticationTime2", kMockElapsedTime,
      1);
}

// Test that unfinished authentication is cancelled if the "Fill password"
// suggestion is accepted again.
TEST_F(PasswordManualFallbackFlowTest, CancelsAuthIfPreviousNotFinished) {
  InitializeFlow();
  ProcessPasswordStoreUpdates();

  FieldRendererId field_id = MakeFieldRendererId();
  flow().RunFlow(field_id, gfx::RectF{}, TextDirection::LEFT_TO_RIGHT);

  auto authenticator1 =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  auto authenticator1_ptr = authenticator1.get();
  EXPECT_CALL(*authenticator1, AuthenticateWithMessage);

  auto authenticator2 =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  EXPECT_CALL(*authenticator2, AuthenticateWithMessage);

  EXPECT_CALL(password_manager_client(), IsReauthBeforeFillingRequired)
      .Times(2)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(password_manager_client(), GetDeviceAuthenticator)
      .Times(2)
      .WillOnce(Return(testing::ByMove(std::move(authenticator1))))
      .WillOnce(Return(testing::ByMove(std::move(authenticator2))));
  ShowAndAcceptSuggestion(autofill::test::CreateAutofillSuggestion(
                              SuggestionType::kFillPassword, u"Fill password",
                              CreateTestPasswordDetails()),
                          AutofillSuggestionDelegate::SuggestionMetadata{
                              .row = 0, .sub_popup_level = 1});

  EXPECT_CALL(*authenticator1_ptr, Cancel);
  ShowAndAcceptSuggestion(autofill::test::CreateAutofillSuggestion(
                              SuggestionType::kFillPassword, u"Fill password",
                              CreateTestPasswordDetails()),
                          AutofillSuggestionDelegate::SuggestionMetadata{
                              .row = 0, .sub_popup_level = 1});
}

// Test that unfinished authentication is cancelled if the flow object is
// destroyed.
TEST_F(PasswordManualFallbackFlowTest, CancelsAuthOnDestroy) {
  InitializeFlow();
  ProcessPasswordStoreUpdates();

  FieldRendererId field_id = MakeFieldRendererId();
  flow().RunFlow(field_id, gfx::RectF{}, TextDirection::LEFT_TO_RIGHT);

  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  auto authenticator_ptr = authenticator.get();
  EXPECT_CALL(*authenticator, AuthenticateWithMessage);

  EXPECT_CALL(password_manager_client(), IsReauthBeforeFillingRequired)
      .WillOnce(Return(true));
  EXPECT_CALL(password_manager_client(), GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(authenticator))));
  ShowAndAcceptSuggestion(autofill::test::CreateAutofillSuggestion(
                              SuggestionType::kFillPassword, u"Fill password",
                              CreateTestPasswordDetails()),
                          AutofillSuggestionDelegate::SuggestionMetadata{
                              .row = 0, .sub_popup_level = 1});

  EXPECT_CALL(*authenticator_ptr, Cancel);
}

// Test that selecting "Manage passwords" suggestion doesn't trigger navigation.
TEST_F(PasswordManualFallbackFlowTest, SelectManagePasswordsEntry) {
  InitializeFlow();
  ProcessPasswordStoreUpdates();

  flow().RunFlow(MakeFieldRendererId(), gfx::RectF{},
                 TextDirection::LEFT_TO_RIGHT);

  EXPECT_CALL(password_manager_client(), NavigateToManagePasswordsPage)
      .Times(0);
  base::HistogramTester histograms;
  flow().DidSelectSuggestion(autofill::test::CreateAutofillSuggestion(
      SuggestionType::kAllSavedPasswordsEntry, u"Manage passwords"));
  histograms.ExpectUniqueSample(
      "PasswordManager.PasswordDropdownItemSelected",
      metrics_util::PasswordDropdownSelectedOption::kShowAll, 0);
}

// Test that selecting "Manage passwords" suggestion triggers page navigation
// and metric recording.
TEST_F(PasswordManualFallbackFlowTest, AcceptManagePasswordsEntry) {
  InitializeFlow();
  ProcessPasswordStoreUpdates();

  flow().RunFlow(MakeFieldRendererId(), gfx::RectF{},
                 TextDirection::LEFT_TO_RIGHT);

  EXPECT_CALL(password_manager_client(),
              NavigateToManagePasswordsPage(
                  ManagePasswordsReferrer::kPasswordDropdown));
  base::HistogramTester histograms;
  ShowAndAcceptSuggestion(
      autofill::test::CreateAutofillSuggestion(
          SuggestionType::kAllSavedPasswordsEntry, u"Manage passwords"),
      AutofillSuggestionDelegate::SuggestionMetadata{.row = 1,
                                                     .sub_popup_level = 0});
  histograms.ExpectUniqueSample(
      "PasswordManager.PasswordDropdownItemSelected",
      metrics_util::PasswordDropdownSelectedOption::kShowAll, 1);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_CHROMEOS)
TEST_F(PasswordManualFallbackFlowTest, ShowPasswordDetails) {
  PasswordForm form_com =
      CreateEntry("username@google.com", "password",
                  GURL("https://google.com/"), PasswordForm::MatchType::kExact);
  PasswordForm form_de =
      CreateEntry("username@google.com", "password", GURL("https://google.de/"),
                  PasswordForm::MatchType::kExact);
  profile_password_store().AddLogins({form_com, form_de});

  InitializeFlow();
  ProcessPasswordStoreUpdates();

  flow().RunFlow(MakeFieldRendererId(), gfx::RectF{},
                 TextDirection::LEFT_TO_RIGHT);

  EXPECT_CALL(password_manager_client(), OpenPasswordDetailsBubble(form_de));
  ShowAndAcceptSuggestion(
      autofill::test::CreateAutofillSuggestion(
          SuggestionType::kViewPasswordDetails, u"View details",
          Suggestion::PasswordSuggestionDetails(
              u"username@google.com", u"password", "https://google.de/",
              u"google.de", false)),
      AutofillSuggestionDelegate::SuggestionMetadata{.row = 0,
                                                     .sub_popup_level = 1});
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_CHROMEOS)

// This class tests that "FillAfterSuggestion" password metrics are recorded
// correctly.
// The first parameter determines whether the suggestion is accepted or not.
// The second parameter determines whether the field is classified as target
// filling password or not.
// The third parameter determines whether the suggestion is taken from a search
// result list.
class PasswordManualFallbackFlowFillAfterSuggestionMetricsTest
    : public PasswordManualFallbackFlowTest,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  // If true, the test will simulate both showing and accepting a suggestion. If
  // false, the test will simulate only showing the suggestion.
  bool SuggestionAccepted() const { return std::get<0>(GetParam()); }

  bool SuggestionFromSearchResult() const { return std::get<2>(GetParam()); }

  bool IsClassifiedAsTargetFillingPassword() const {
    return std::get<1>(GetParam());
  }

  // The metric name is dependent on whether the field is classified as target
  // filling password or not.
  std::string MetricName() const {
    auto metric_name = [](std::string_view token) {
      return base::StrCat(
          {"Autofill.Funnel.", token, ".FillAfterSuggestion.Password"});
    };
    if (IsClassifiedAsTargetFillingPassword()) {
      return metric_name("ClassifiedAsTargetFilling");
    } else {
      return metric_name("NotClassifiedAsTargetFilling");
    }
  }

  void ResetFlowAndMetricsRecorder() {
    // Reset `flow_` first since it hold a raw pointer to
    // `manual_fallback_metrics_recorder_`. In production, `flow_` and
    // `manual_fallback_metrics_recorder_` always die at the same time.
    flow_.reset();
    manual_fallback_metrics_recorder_.reset();
  }
};

// Tests that top level suggestions are acceptable when suggestions are
// triggered on a login form.
TEST_F(PasswordManualFallbackFlowTest, Acceptability_OnLoginForm) {
  InitializeFlow();
  ProcessPasswordStoreUpdates();

  PasswordForm form;
  form.username_element_renderer_id = MakeFieldRendererId();
  form.password_element_renderer_id = MakeFieldRendererId();
  // Simulate that the field is classified as target filling password.
  EXPECT_CALL(password_form_cache(),
              GetPasswordForm(_, form.username_element_renderer_id))
      .WillRepeatedly(Return(&form));

  EXPECT_CALL(
      autofill_client(),
      ShowAutofillSuggestions(
          AllOf(Field(
              "suggestions", &AutofillClient::PopupOpenArgs::suggestions,
              ElementsAre(
                  EqualsSuggestion(SuggestionType::kTitle),
                  EqualsManualFallbackSuggestion(SuggestionType::kPasswordEntry,
                                                 /*is_acceptable=*/true),
                  EqualsSuggestion(SuggestionType::kTitle),
                  EqualsManualFallbackSuggestion(SuggestionType::kPasswordEntry,
                                                 /*is_acceptable=*/true),
                  EqualsSuggestion(SuggestionType::kSeparator),
                  EqualsSuggestion(SuggestionType::kAllSavedPasswordsEntry)))),
          _));

  flow().RunFlow(form.username_element_renderer_id, gfx::RectF{},
                 TextDirection::LEFT_TO_RIGHT);
}

// Tests that top level suggestions are not acceptable when suggestions are
// triggered on a signup form.
TEST_F(PasswordManualFallbackFlowTest, Acceptability_OnSignupForm) {
  InitializeFlow();
  ProcessPasswordStoreUpdates();

  PasswordForm form;
  form.username_element_renderer_id = MakeFieldRendererId();
  form.new_password_element_renderer_id = MakeFieldRendererId();
  // Simulate that the field is classified as target filling password.
  EXPECT_CALL(password_form_cache(),
              GetPasswordForm(_, form.username_element_renderer_id))
      .WillRepeatedly(Return(&form));

  EXPECT_CALL(
      autofill_client(),
      ShowAutofillSuggestions(
          AllOf(Field(
              "suggestions", &AutofillClient::PopupOpenArgs::suggestions,
              ElementsAre(
                  EqualsSuggestion(SuggestionType::kTitle),
                  EqualsManualFallbackSuggestion(SuggestionType::kPasswordEntry,
                                                 /*is_acceptable=*/false),
                  EqualsSuggestion(SuggestionType::kTitle),
                  EqualsManualFallbackSuggestion(SuggestionType::kPasswordEntry,
                                                 /*is_acceptable=*/false),
                  EqualsSuggestion(SuggestionType::kSeparator),
                  EqualsSuggestion(SuggestionType::kAllSavedPasswordsEntry)))),
          _));

  flow().RunFlow(form.username_element_renderer_id, gfx::RectF{},
                 TextDirection::LEFT_TO_RIGHT);
}

// Tests that top level suggestions are not acceptable when suggestions are
// triggered from a field that is not the main username or password field.
TEST_F(PasswordManualFallbackFlowTest, Acceptability_IrrelevantFocusedElement) {
  InitializeFlow();
  ProcessPasswordStoreUpdates();

  PasswordForm form;
  form.username_element_renderer_id = MakeFieldRendererId();
  form.password_element_renderer_id = MakeFieldRendererId();
  FieldRendererId other_field_id = MakeFieldRendererId();
  // Simulate that the field is classified as target filling password.
  EXPECT_CALL(password_form_cache(), GetPasswordForm(_, other_field_id))
      .WillRepeatedly(Return(&form));

  EXPECT_CALL(
      autofill_client(),
      ShowAutofillSuggestions(
          AllOf(Field(
              "suggestions", &AutofillClient::PopupOpenArgs::suggestions,
              ElementsAre(
                  EqualsSuggestion(SuggestionType::kTitle),
                  EqualsManualFallbackSuggestion(SuggestionType::kPasswordEntry,
                                                 /*is_acceptable=*/false),
                  EqualsSuggestion(SuggestionType::kTitle),
                  EqualsManualFallbackSuggestion(SuggestionType::kPasswordEntry,
                                                 /*is_acceptable=*/false),
                  EqualsSuggestion(SuggestionType::kSeparator),
                  EqualsSuggestion(SuggestionType::kAllSavedPasswordsEntry)))),
          _));

  flow().RunFlow(other_field_id, gfx::RectF{}, TextDirection::LEFT_TO_RIGHT);
}

TEST_P(PasswordManualFallbackFlowFillAfterSuggestionMetricsTest,
       MetricsAreRecorded) {
  InitializeFlow();
  ProcessPasswordStoreUpdates();

  PasswordForm form;
  form.username_element_renderer_id = MakeFieldRendererId();
  form.password_element_renderer_id = MakeFieldRendererId();
  // Simulate that the field is/isn't classified as target filling password.
  EXPECT_CALL(password_form_cache(),
              GetPasswordForm(_, form.username_element_renderer_id))
      .WillRepeatedly(
          Return(IsClassifiedAsTargetFillingPassword() ? &form : nullptr));

  flow().RunFlow(form.username_element_renderer_id, gfx::RectF{},
                 TextDirection::LEFT_TO_RIGHT);

  base::HistogramTester histograms;
  autofill::Suggestion suggestion = autofill::test::CreateAutofillSuggestion(
      SuggestionType::kPasswordFieldByFieldFilling, u"password");
  if (SuggestionAccepted()) {
    ShowAndAcceptSuggestion(
        suggestion, AutofillSuggestionDelegate::SuggestionMetadata{
                        .row = 0,
                        .sub_popup_level = 0,
                        .from_search_result = SuggestionFromSearchResult()});
    histograms.ExpectUniqueSample(
        "PasswordManager.ManualFallback.AcceptedSuggestion.SearchInputUsed",
        SuggestionFromSearchResult(), 1);
  } else {
    flow().OnSuggestionsShown(base::span_from_ref(suggestion));
  }

  // The metric of the metrics recorder is recorded only in the destructor.
  histograms.ExpectTotalCount(MetricName(), 0);
  ResetFlowAndMetricsRecorder();
  histograms.ExpectUniqueSample(MetricName(), SuggestionAccepted(), 1);
}

INSTANTIATE_TEST_SUITE_P(
    PasswordManualFallbackFlowTest,
    PasswordManualFallbackFlowFillAfterSuggestionMetricsTest,
    ::testing::Combine(testing::Bool(), testing::Bool(), testing::Bool()),
    [](const testing::TestParamInfo<std::tuple<bool, bool, bool>>& info) {
      return base::StrCat(
          {std::get<0>(info.param) ? "SuggestionAccepted" : "SuggestionShown",
           std::get<1>(info.param) ? "_ClassifiedAsTargetFilling"
                                   : "_NotClassifiedAsTargetFilling",
           std::get<2>(info.param) ? "_WithSearchInput" : "_NoSearchInput"});
    });

}  // namespace

}  // namespace password_manager
