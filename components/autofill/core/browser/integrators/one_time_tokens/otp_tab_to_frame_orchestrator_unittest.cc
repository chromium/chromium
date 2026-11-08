// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_tab_to_frame_orchestrator.h"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/foundations/autofill_manager_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_coordinator.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_phish_guard_delegate.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_util.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/one_time_tokens/core/browser/fake_sms_otp_service.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {
namespace {

using ::autofill::test::FormDescription;
using ::autofill::test::GetServerTypes;
using ::one_time_tokens::OneTimeToken;
using ::one_time_tokens::OneTimeTokenType;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Property;
using ::testing::Test;

constexpr char kTestUrl[] = "https://example.test";

auto HasValue(std::string_view code) {
  return ElementsAre(Property(&OneTimeToken::value, code));
}

class RecordingAutofillDriver : public TestAutofillDriver {
 public:
  using TestAutofillDriver::TestAutofillDriver;

  void RendererShouldTriggerSuggestions(
      const FieldGlobalId& field_id,
      AutofillSuggestionTriggerSource trigger_source) override {
    if (trigger_source ==
        AutofillSuggestionTriggerSource::kGmailOneTimePasswordAvailable) {
      triggered_fields_.push_back(field_id);
    }
  }

  const std::vector<FieldGlobalId>& triggered_fields() const {
    return triggered_fields_;
  }
  void clear_triggered_fields() { triggered_fields_.clear(); }

 private:
  std::vector<FieldGlobalId> triggered_fields_;
};

class FakeOtpPhishGuardDelegate : public OtpPhishGuardDelegate {
 public:
  void StartOtpPhishGuardCheck(
      LocalFrameToken,
      base::OnceCallback<void(bool is_phishing)> callback) override {
    std::move(callback).Run(/*is_phishing=*/false);
  }
};

class OtpTabToFrameOrchestratorTest
    : public Test,
      public WithTestAutofillClientDriverManager<TestAutofillClient,
                                                 RecordingAutofillDriver> {
 protected:
  void SetUp() override {
    InitAutofillClient();
    autofill_client().set_last_committed_primary_main_frame_url(GURL(kTestUrl));

    auto sms_service = std::make_unique<one_time_tokens::FakeSmsOtpService>();
    sms_otp_service_ = sms_service.get();
    autofill_client().set_sms_otp_service(std::move(sms_service));
    autofill_client().set_otp_phish_guard_delegate(
        std::make_unique<FakeOtpPhishGuardDelegate>());

    CreateAutofillDriver();
    coordinator_ = std::make_unique<OtpCoordinator>(&autofill_client());
    orchestrator_ = std::make_unique<OtpTabToFrameOrchestrator>(
        &autofill_manager(), coordinator_.get());
  }

  void TearDown() override {
    orchestrator_.reset();
    coordinator_.reset();
    sms_otp_service_ = nullptr;
    DestroyAutofillClient();
  }

  const FormStructure& AddOtpForm(size_t fields = 1,
                                  GURL url = GURL(kTestUrl)) {
    FormDescription form_description = {
        .host_frame = autofill_driver().GetFrameToken(),
        .main_frame_origin = url::Origin::Create(url),
    };
    for (size_t i = 0; i < fields; ++i) {
      form_description.fields.push_back(
          {.server_type = ONE_TIME_CODE,
           .host_frame = autofill_driver().GetFrameToken(),
           .origin = url::Origin::Create(url)});
    }
    FormData form = test::GetFormData(form_description);
    FormGlobalId form_id = form.global_id();
    auto form_structure = std::make_unique<FormStructure>(form);
    test_api(*form_structure).SetFieldTypes(GetServerTypes(form_description));
    test_api(*form_structure).AssignSections();
    test_api(autofill_manager())
        .AddSeenFormStructure(std::move(form_structure));
    test_api(autofill_manager()).OnFormsParsed({form});

    orchestrator_->OnFieldTypesDetermined(
        autofill_manager(), form_id,
        AutofillManager::Observer::FieldTypeSource::kAutofillServer,
        /*small_forms_were_parsed=*/false);
    return *autofill_manager().FindCachedFormById(form_id);
  }

  std::vector<OneTimeToken> QuerySuggestions(const FormStructure& form,
                                             const FormFieldData& field) {
    base::test::TestFuture<std::vector<OneTimeToken>> future;
    orchestrator_->GetOtpSuggestions(form, field, future.GetCallback());
    return future.Take();
  }

  void SendSmsOtp(const std::string& code) {
    task_environment_.AdvanceClock(base::Milliseconds(1));
    sms_otp_service_->SimulateOtpArrived(
        OneTimeToken(OneTimeTokenType::kSmsOtp, code, base::TimeTicks::Now()));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  raw_ptr<one_time_tokens::FakeSmsOtpService> sms_otp_service_ = nullptr;
  std::unique_ptr<OtpCoordinator> coordinator_;
  std::unique_ptr<OtpTabToFrameOrchestrator> orchestrator_;
};

// Tests that OTP suggestions are served on all fields of an OTP form and that
// a proactive dropdown trigger is dispatched to the focused field.
TEST_F(OtpTabToFrameOrchestratorTest,
       ServesSuggestionsAndTriggersProactivelyOnFocusedField) {
  const FormStructure& form = AddOtpForm(/*fields=*/2);
  const AutofillField* field0 = form.field(0);
  const AutofillField* field1 = form.field(1);

  EXPECT_THAT(QuerySuggestions(form, *field0), IsEmpty());

  orchestrator_->OnBeforeFocusOnFormField(autofill_manager(), form.global_id(),
                                          field1->global_id());
  SendSmsOtp("654321");

  EXPECT_THAT(autofill_driver().triggered_fields(),
              ElementsAre(field1->global_id()));
  EXPECT_THAT(QuerySuggestions(form, *field0), HasValue("654321"));
  EXPECT_THAT(QuerySuggestions(form, *field1), HasValue("654321"));
}

// Tests that recording an OTP suggestion as displayed on a field suppresses
// subsequent proactive dropdown triggers for the same token value.
TEST_F(OtpTabToFrameOrchestratorTest,
       OnSuggestionsShownSuppressesProactiveTriggerForSameCode) {
  const FormStructure& form = AddOtpForm(/*fields=*/2);
  const AutofillField* field0 = form.field(0);

  // Focus a non-OTP field so the first arrival of "654321" does not trigger a
  // proactive popup.
  FieldGlobalId non_otp_field(autofill_driver().GetFrameToken(),
                              FieldRendererId(999));
  orchestrator_->OnBeforeFocusOnFormField(autofill_manager(), form.global_id(),
                                          non_otp_field);
  SendSmsOtp("654321");
  EXPECT_THAT(autofill_driver().triggered_fields(), IsEmpty());

  // Query suggestions on `field0`, mark "654321" as shown, focus `field0`, and
  // resend "654321": no proactive trigger should fire.
  EXPECT_THAT(QuerySuggestions(form, *field0), HasValue("654321"));
  orchestrator_->OnSuggestionsShown(
      autofill_manager(),
      {Suggestion(u"654321", SuggestionType::kOneTimePasswordEntry)});
  orchestrator_->OnBeforeFocusOnFormField(autofill_manager(), form.global_id(),
                                          field0->global_id());
  SendSmsOtp("654321");
  EXPECT_THAT(autofill_driver().triggered_fields(), IsEmpty());
}

// Tests that proactive dropdown triggers are suppressed when a non-OTP field
// has focus, while suggestions remain available reactively.
TEST_F(OtpTabToFrameOrchestratorTest, SuppressesProactiveTriggerOnNonOtpFocus) {
  const FormStructure& form = AddOtpForm();
  const AutofillField* field = form.field(0);

  FieldGlobalId non_otp_field(autofill_driver().GetFrameToken(),
                              FieldRendererId(999));
  orchestrator_->OnBeforeFocusOnFormField(autofill_manager(), form.global_id(),
                                          non_otp_field);

  SendSmsOtp("111111");
  EXPECT_THAT(autofill_driver().triggered_fields(), IsEmpty());
  EXPECT_THAT(QuerySuggestions(form, *field), HasValue("111111"));
}

// Tests that submitting an OTP form consumes the submitted value and resets
// typing state, allowing subsequent forms to receive proactive triggers.
TEST_F(OtpTabToFrameOrchestratorTest,
       OnBeforeFormSubmittedConsumesSubmittedCodeAndResetsTypingState) {
  const FormStructure& form = AddOtpForm();
  const AutofillField* field = form.field(0);

  FieldGlobalId non_otp_field(autofill_driver().GetFrameToken(),
                              FieldRendererId(999));
  orchestrator_->OnAfterTextFieldValueChanged(autofill_manager(),
                                              form.global_id(), non_otp_field);
  SendSmsOtp("222222");
  EXPECT_THAT(autofill_driver().triggered_fields(), IsEmpty());
  EXPECT_THAT(QuerySuggestions(form, *field), HasValue("222222"));

  // Submitting `form` consumes its typed OTP value and clears typing state,
  // allowing a proactive trigger on `form2` when a new OTP arrives.
  FormData submitted_form = form.ToFormData();
  test_api(submitted_form).field(0).set_value(u"222222");
  orchestrator_->OnBeforeFormSubmitted(autofill_manager(), submitted_form);
  EXPECT_THAT(QuerySuggestions(form, *field), IsEmpty());

  test_api(autofill_manager()).ClearFormStructures();
  const FormStructure& form2 = AddOtpForm();
  EXPECT_THAT(QuerySuggestions(form2, *form2.field(0)), IsEmpty());
  SendSmsOtp("222222");
  EXPECT_THAT(QuerySuggestions(form2, *form2.field(0)), IsEmpty());

  orchestrator_->OnBeforeFocusOnFormField(autofill_manager(), form2.global_id(),
                                          form2.field(0)->global_id());
  SendSmsOtp("333333");
  EXPECT_THAT(autofill_driver().triggered_fields(),
              ElementsAre(form2.field(0)->global_id()));
  EXPECT_THAT(QuerySuggestions(form2, *form2.field(0)), HasValue("333333"));
}

// Tests that destroying and recreating the frame orchestrator retains token
// eligibility and cached tokens via the tab coordinator.
TEST_F(OtpTabToFrameOrchestratorTest,
       RetainsEligibilityAndCachedTokensAcrossOrchestratorRecreation) {
  const FormStructure& form = AddOtpForm();
  SendSmsOtp("333333");
  EXPECT_THAT(QuerySuggestions(form, *form.field(0)), HasValue("333333"));

  // Match `BrowserAutofillManager::Reset()`, which constructs the new
  // orchestrator before destroying the old one.
  test_api(autofill_manager()).ClearFormStructures();
  orchestrator_ = std::make_unique<OtpTabToFrameOrchestrator>(
      &autofill_manager(), coordinator_.get());
  const FormStructure& form2 = AddOtpForm();
  EXPECT_THAT(QuerySuggestions(form2, *form2.field(0)), HasValue("333333"));
}

// Tests that inactive frames in BFCache drop deliveries and that restoring
// from BFCache rescans cached forms and resumes OTP listening.
TEST_F(OtpTabToFrameOrchestratorTest,
       RestoringFromBFCacheRescansFormsAndResumesListening) {
  const FormStructure& form = AddOtpForm();
  SendSmsOtp("333333");
  EXPECT_THAT(QuerySuggestions(form, *form.field(0)), HasValue("333333"));

  autofill_driver().SetIsActive(false);
  orchestrator_->OnAutofillManagerStateChanged(
      autofill_manager(), AutofillManager::LifecycleState::kActive,
      AutofillManager::LifecycleState::kInactive);
  EXPECT_THAT(QuerySuggestions(form, *form.field(0)), IsEmpty());

  autofill_driver().SetIsActive(true);
  orchestrator_->OnAutofillManagerStateChanged(
      autofill_manager(), AutofillManager::LifecycleState::kInactive,
      AutofillManager::LifecycleState::kActive);
  EXPECT_THAT(QuerySuggestions(form, *form.field(0)), HasValue("333333"));
}

// Tests that the orchestrator stops listening when field classifications change
// to non-OTP or when forms are removed from the manager.
TEST_F(OtpTabToFrameOrchestratorTest,
       StopsListeningWhenFieldTypeChangesOrFormIsRemoved) {
  const FormStructure& form = AddOtpForm();
  EXPECT_EQ(sms_otp_service_->num_subscribers(), 1u);

  test_api(const_cast<FormStructure&>(form)).SetFieldTypes({NAME_FIRST});
  orchestrator_->OnFieldTypesDetermined(
      autofill_manager(), form.global_id(),
      AutofillManager::Observer::FieldTypeSource::kAutofillServer,
      /*small_forms_were_parsed=*/false);
  EXPECT_EQ(sms_otp_service_->num_subscribers(), 0u);

  const FormStructure& form2 = AddOtpForm();
  EXPECT_EQ(sms_otp_service_->num_subscribers(), 1u);
  orchestrator_->OnAfterFormsSeen(autofill_manager(), /*updated_forms=*/{},
                                  /*removed_forms=*/{form2.global_id()});
  EXPECT_EQ(sms_otp_service_->num_subscribers(), 0u);
}

}  // namespace
}  // namespace autofill
