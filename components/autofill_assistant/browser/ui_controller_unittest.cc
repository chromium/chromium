// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/ui_controller.h"

#include <memory>
#include <utility>

#include "base/callback_helpers.h"
#include "base/containers/flat_map.h"
#include "base/guid.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill_assistant/browser/cud_condition.pb.h"
#include "components/autofill_assistant/browser/device_context.h"
#include "components/autofill_assistant/browser/fake_script_executor_ui_delegate.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/mock_autofill_assistant_tts_controller.h"
#include "components/autofill_assistant/browser/mock_client.h"
#include "components/autofill_assistant/browser/mock_execution_delegate.h"
#include "components/autofill_assistant/browser/mock_ui_controller_observer.h"
#include "components/autofill_assistant/browser/test_util.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_assistant {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Gt;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Property;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::Sequence;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;
using ::testing::WithArgs;

namespace {

constexpr char kClientLocale[] = "en-US";

// Same as non-mock, but provides default mock callbacks.
struct FakeCollectUserDataOptions : public CollectUserDataOptions {
  FakeCollectUserDataOptions() {
    confirm_callback = base::DoNothing();
    additional_actions_callback = base::DoNothing();
    terms_link_callback = base::DoNothing();
    selected_user_data_changed_callback = base::DoNothing();
  }
};

}  // namespace

class UiControllerTest : public testing::Test {
 public:
  UiControllerTest() {}

  void SetUp() override {
    ON_CALL(mock_client_, GetLocale()).WillByDefault(Return(kClientLocale));
    ON_CALL(mock_execution_delegate_, GetUserModel())
        .WillByDefault(Return(&user_model_));
    ON_CALL(mock_execution_delegate_, GetUserData())
        .WillByDefault(Return(&user_data_));
    ON_CALL(mock_execution_delegate_, GetClientSettings())
        .WillByDefault(ReturnRef(client_settings_));
    ON_CALL(mock_execution_delegate_, GetCurrentURL())
        .WillByDefault(ReturnRef(current_url_));
    ON_CALL(mock_execution_delegate_, GetTriggerContext())
        .WillByDefault(Return(&trigger_context_));

    auto tts_controller =
        std::make_unique<NiceMock<MockAutofillAssistantTtsController>>();
    mock_tts_controller_ = tts_controller.get();
    ui_controller_ = std::make_unique<UiController>(
        &mock_client_, &mock_execution_delegate_, std::move(tts_controller));

    ui_controller_->AddObserver(&mock_observer_);
    ui_controller_->StartListening();
  }

  void TearDown() override {
    ui_controller_->RemoveObserver(&mock_observer_);
    ui_controller_.reset();
  }

 protected:
  RequiredDataPiece MakeRequiredDataPiece(autofill::ServerFieldType field) {
    RequiredDataPiece required_data_piece;
    required_data_piece.mutable_condition()->set_key(static_cast<int>(field));
    required_data_piece.mutable_condition()->mutable_not_empty();
    return required_data_piece;
  }

  void EnableTtsForTest() { ui_controller_->tts_enabled_ = true; }

  void SetTtsButtonStateForTest(TtsButtonState state) {
    ui_controller_->tts_button_state_ = state;
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  NiceMock<MockClient> mock_client_;
  NiceMock<MockExecutionDelegate> mock_execution_delegate_;
  raw_ptr<MockAutofillAssistantTtsController> mock_tts_controller_;
  NiceMock<MockUiControllerObserver> mock_observer_;
  std::unique_ptr<UiController> ui_controller_;
  UserModel user_model_;
  UserData user_data_;
  TriggerContext trigger_context_;
  ClientSettings client_settings_;
  base::test::ScopedFeatureList scoped_feature_list_;
  GURL current_url_ = GURL("http://www.example.com");
};

UserAction MakeUserAction(const std::string& text) {
  ChipProto chip;
  chip.set_text(text);

  return UserAction(chip, true, "");
}

TEST_F(UiControllerTest, ClearUserActionsOnSelection) {
  {
    testing::InSequence seq;
    // We set two chips.
    EXPECT_CALL(mock_observer_, OnUserActionsChanged(SizeIs(2)));
    // When one chip is selected the user actions are cleared.
    EXPECT_CALL(mock_observer_, OnUserActionsChanged(SizeIs(0)));
  }
  auto actions = std::make_unique<std::vector<UserAction>>();
  actions->push_back(MakeUserAction("Continue"));
  actions->push_back(MakeUserAction("Cancel"));
  ui_controller_->SetUserActions(std::move(actions));

  EXPECT_TRUE(ui_controller_->PerformUserAction(0));
}

TEST_F(UiControllerTest, StopWithFeedbackChip) {
  client_settings_.display_strings[ClientSettingsProto::SEND_FEEDBACK] =
      "send_feedback";
  // By default the feedback chip isn't shown on stop.
  ui_controller_->OnStop();
  ASSERT_THAT(ui_controller_->GetUserActions(), SizeIs(0u));

  // If requested, the feedback chip should be shown.
  ui_controller_->SetShowFeedbackChip(true);
  ui_controller_->OnStop();
  EXPECT_THAT(
      ui_controller_->GetUserActions(),
      ElementsAre(Property(&UserAction::chip,
                           AllOf(Field(&Chip::type, FEEDBACK_ACTION),
                                 Field(&Chip::text, "send_feedback")))));
}

TEST_F(UiControllerTest, FeedbackChipIsShownOnSpecificErrors) {
  client_settings_.display_strings[ClientSettingsProto::SEND_FEEDBACK] =
      "send_feedback";
  // For this dropout reason, the feedback chip should not be shown.
  ui_controller_->OnError("error_message", Metrics::DropOutReason::NO_SCRIPTS);
  ui_controller_->OnStop();
  ASSERT_THAT(ui_controller_->GetUserActions(), SizeIs(0u));

  // For this dropout reason, the feedback chip should be shown.
  ui_controller_->OnError("error_message",
                          Metrics::DropOutReason::GET_SCRIPTS_FAILED);
  ui_controller_->OnStop();
  EXPECT_THAT(
      ui_controller_->GetUserActions(),
      ElementsAre(Property(&UserAction::chip,
                           AllOf(Field(&Chip::type, FEEDBACK_ACTION),
                                 Field(&Chip::text, "send_feedback")))));
}

TEST_F(UiControllerTest, FeedbackChipNotShownWithFeatureFlagDisabled) {
  // Disable the feedback chip feature.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(
      features::kAutofillAssistantFeedbackChip);

  // With the feature disabled, the chip shouldn't be shown even if requested.
  ui_controller_->SetShowFeedbackChip(true);
  ui_controller_->OnStop();
  ASSERT_THAT(ui_controller_->GetUserActions(), SizeIs(0u));
}

TEST_F(UiControllerTest, ProgressSetAtStart) {
  EXPECT_CALL(mock_observer_, OnStepProgressBarConfigurationChanged(_));
  EXPECT_CALL(mock_observer_, OnProgressActiveStepChanged(0));
  ui_controller_->OnStart(trigger_context_);
  EXPECT_EQ(0, ui_controller_->GetProgressActiveStep());
}

TEST_F(UiControllerTest, SetProgressStep) {
  EXPECT_CALL(mock_observer_, OnStepProgressBarConfigurationChanged(_));
  EXPECT_CALL(mock_observer_, OnProgressActiveStepChanged(0));
  ui_controller_->OnStart(trigger_context_);

  ShowProgressBarProto::StepProgressBarConfiguration config;
  config.add_annotated_step_icons()->set_identifier("icon1");
  config.add_annotated_step_icons()->set_identifier("icon2");
  EXPECT_CALL(mock_observer_, OnStepProgressBarConfigurationChanged(_));
  EXPECT_CALL(mock_observer_, OnProgressActiveStepChanged(0));
  ui_controller_->SetStepProgressBarConfiguration(config);
  EXPECT_EQ(0, ui_controller_->GetProgressActiveStep());

  EXPECT_CALL(mock_observer_, OnProgressActiveStepChanged(1));
  ui_controller_->SetProgressActiveStep(1);
  EXPECT_EQ(1, ui_controller_->GetProgressActiveStep());
}

TEST_F(UiControllerTest, IgnoreProgressStepDecreases) {
  EXPECT_CALL(mock_observer_, OnProgressActiveStepChanged(0));
  ui_controller_->OnStart(trigger_context_);

  EXPECT_CALL(mock_observer_, OnStepProgressBarConfigurationChanged(_));
  EXPECT_CALL(mock_observer_, OnProgressActiveStepChanged(0));
  ShowProgressBarProto::StepProgressBarConfiguration config;
  config.add_annotated_step_icons()->set_identifier("icon1");
  config.add_annotated_step_icons()->set_identifier("icon2");
  ui_controller_->SetStepProgressBarConfiguration(config);

  EXPECT_CALL(mock_observer_, OnProgressActiveStepChanged(Not(1)))
      .Times(AnyNumber());
  ui_controller_->SetProgressActiveStep(2);
  ui_controller_->SetProgressActiveStep(1);
}

TEST_F(UiControllerTest, NewProgressStepConfigurationClampsStep) {
  ui_controller_->OnStart(trigger_context_);

  ShowProgressBarProto::StepProgressBarConfiguration config;
  config.add_annotated_step_icons()->set_identifier("icon1");
  config.add_annotated_step_icons()->set_identifier("icon2");
  config.add_annotated_step_icons()->set_identifier("icon3");
  ui_controller_->SetStepProgressBarConfiguration(config);

  EXPECT_CALL(mock_observer_, OnProgressActiveStepChanged(3));
  ui_controller_->SetProgressActiveStep(3);
  EXPECT_EQ(3, ui_controller_->GetProgressActiveStep());

  ShowProgressBarProto::StepProgressBarConfiguration new_config;
  new_config.add_annotated_step_icons()->set_identifier("icon1");
  new_config.add_annotated_step_icons()->set_identifier("icon2");
  EXPECT_CALL(mock_observer_, OnProgressActiveStepChanged(2));
  ui_controller_->SetStepProgressBarConfiguration(new_config);
  EXPECT_EQ(2, ui_controller_->GetProgressActiveStep());
}

TEST_F(UiControllerTest, ProgressStepWrapsNegativesToMax) {
  ui_controller_->OnStart(trigger_context_);

  ShowProgressBarProto::StepProgressBarConfiguration config;
  config.add_annotated_step_icons()->set_identifier("icon1");
  config.add_annotated_step_icons()->set_identifier("icon2");
  config.add_annotated_step_icons()->set_identifier("icon3");
  ui_controller_->SetStepProgressBarConfiguration(config);

  EXPECT_CALL(mock_observer_, OnProgressActiveStepChanged(3));
  ui_controller_->SetProgressActiveStep(-1);
  EXPECT_EQ(3, ui_controller_->GetProgressActiveStep());
}

TEST_F(UiControllerTest, ProgressStepClampsOverflowToMax) {
  ui_controller_->OnStart(trigger_context_);

  ShowProgressBarProto::StepProgressBarConfiguration config;
  config.add_annotated_step_icons()->set_identifier("icon1");
  config.add_annotated_step_icons()->set_identifier("icon2");
  config.add_annotated_step_icons()->set_identifier("icon3");
  ui_controller_->SetStepProgressBarConfiguration(config);

  EXPECT_CALL(mock_observer_, OnProgressActiveStepChanged(3));
  ui_controller_->SetProgressActiveStep(std::numeric_limits<int>::max());
  EXPECT_EQ(3, ui_controller_->GetProgressActiveStep());
}

TEST_F(UiControllerTest, SetProgressStepFromIdentifier) {
  ui_controller_->OnStart(trigger_context_);

  ShowProgressBarProto::StepProgressBarConfiguration config;
  config.add_annotated_step_icons()->set_identifier("icon1");
  config.add_annotated_step_icons()->set_identifier("icon2");
  ui_controller_->SetStepProgressBarConfiguration(config);

  EXPECT_CALL(mock_observer_, OnProgressActiveStepChanged(1));
  EXPECT_TRUE(ui_controller_->SetProgressActiveStepIdentifier("icon2"));
  EXPECT_EQ(1, ui_controller_->GetProgressActiveStep());
}

TEST_F(UiControllerTest, SetProgressStepFromUnknownIdentifier) {
  EXPECT_CALL(mock_observer_, OnProgressActiveStepChanged(0));
  ui_controller_->OnStart(trigger_context_);
  EXPECT_EQ(0, ui_controller_->GetProgressActiveStep());

  EXPECT_CALL(mock_observer_, OnStepProgressBarConfigurationChanged(_));
  EXPECT_CALL(mock_observer_, OnProgressActiveStepChanged(0));
  ShowProgressBarProto::StepProgressBarConfiguration config;
  config.add_annotated_step_icons()->set_identifier("icon1");
  config.add_annotated_step_icons()->set_identifier("icon2");
  ui_controller_->SetStepProgressBarConfiguration(config);

  EXPECT_CALL(mock_observer_, OnProgressActiveStepChanged(_)).Times(0);
  EXPECT_FALSE(ui_controller_->SetProgressActiveStepIdentifier("icon3"));
  EXPECT_EQ(0, ui_controller_->GetProgressActiveStep());
}

TEST_F(UiControllerTest, UserDataFormEmpty) {
  auto options = std::make_unique<FakeCollectUserDataOptions>();

  // Request nothing, expect continue button to be enabled.
  EXPECT_CALL(mock_observer_, OnCollectUserDataOptionsChanged(Not(nullptr)));
  EXPECT_CALL(mock_execution_delegate_,
              NotifyUserDataChange(UserDataFieldChange::ALL));
  ui_controller_->SetCollectUserDataOptions(options.get());
  EXPECT_CALL(mock_observer_, OnUserActionsChanged(UnorderedElementsAre(
                                  Property(&UserAction::enabled, Eq(true)))));
  ui_controller_->OnUserDataChanged(user_data_, UserDataFieldChange::ALL);
}

TEST_F(UiControllerTest, UserDataFormContactInfo) {
  auto options = std::make_unique<FakeCollectUserDataOptions>();

  options->required_contact_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::NAME_FULL));
  options->required_contact_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::EMAIL_ADDRESS));
  options->required_contact_data_pieces.push_back(MakeRequiredDataPiece(
      autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER));
  options->contact_details_name = "selected_profile";

  testing::InSequence seq;
  EXPECT_CALL(mock_execution_delegate_,
              NotifyUserDataChange(UserDataFieldChange::ALL));
  ui_controller_->SetCollectUserDataOptions(options.get());
  EXPECT_CALL(mock_observer_, OnUserActionsChanged(UnorderedElementsAre(
                                  Property(&UserAction::enabled, Eq(false)))));
  ui_controller_->OnUserDataChanged(user_data_, UserDataFieldChange::ALL);

  EXPECT_CALL(mock_execution_delegate_,
              NotifyUserDataChange(UserDataFieldChange::CONTACT_PROFILE));
  autofill::AutofillProfile contact_profile;
  contact_profile.SetRawInfo(autofill::ServerFieldType::EMAIL_ADDRESS,
                             u"joedoe@example.com");
  contact_profile.SetRawInfo(autofill::ServerFieldType::NAME_FULL, u"Joe Doe");
  contact_profile.SetRawInfo(autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER,
                             u"+1 23 456 789 01");
  ui_controller_->HandleContactInfoChange(
      std::make_unique<autofill::AutofillProfile>(contact_profile), UNKNOWN);

  EXPECT_CALL(mock_observer_, OnUserActionsChanged(UnorderedElementsAre(
                                  Property(&UserAction::enabled, Eq(true)))));
  ui_controller_->OnUserDataChanged(user_data_,
                                    UserDataFieldChange::CONTACT_PROFILE);
  EXPECT_THAT(
      user_data_.selected_address("selected_profile")->Compare(contact_profile),
      Eq(0));
}

TEST_F(UiControllerTest, UserDataFormCreditCard) {
  testing::InSequence seq;

  auto options = std::make_unique<FakeCollectUserDataOptions>();
  options->request_payment_method = true;
  options->billing_address_name = "billing_address";

  EXPECT_CALL(mock_execution_delegate_,
              NotifyUserDataChange(UserDataFieldChange::ALL));
  ui_controller_->SetCollectUserDataOptions(options.get());
  EXPECT_CALL(mock_observer_, OnUserActionsChanged(UnorderedElementsAre(
                                  Property(&UserAction::enabled, Eq(false)))));
  ui_controller_->OnUserDataChanged(user_data_, UserDataFieldChange::ALL);

  // Credit card with valid billing address is ok.
  auto billing_address = std::make_unique<autofill::AutofillProfile>(
      base::GenerateGUID(), "https://www.example.com");
  autofill::test::SetProfileInfo(billing_address.get(), "Marion", "Mitchell",
                                 "Morrison", "marion@me.xyz", "Fox",
                                 "123 Zoo St.", "unit 5", "Hollywood", "CA",
                                 "91601", "US", "16505678910");
  auto credit_card = std::make_unique<autofill::CreditCard>(
      base::GenerateGUID(), "https://www.example.com");
  autofill::test::SetCreditCardInfo(credit_card.get(), "Marion Mitchell",
                                    "4111 1111 1111 1111", "01", "2020",
                                    billing_address->guid());
  EXPECT_CALL(mock_execution_delegate_,
              NotifyUserDataChange(UserDataFieldChange::BILLING_ADDRESS));
  EXPECT_CALL(mock_execution_delegate_,
              NotifyUserDataChange(UserDataFieldChange::CARD));
  ui_controller_->HandleCreditCardChange(
      std::make_unique<autofill::CreditCard>(*credit_card),
      std::make_unique<autofill::AutofillProfile>(*billing_address), UNKNOWN);

  EXPECT_CALL(mock_observer_, OnUserActionsChanged(UnorderedElementsAre(
                                  Property(&UserAction::enabled, Eq(true)))));
  ui_controller_->OnUserDataChanged(user_data_, UserDataFieldChange::CARD);

  EXPECT_THAT(user_data_.selected_card()->Compare(*credit_card), Eq(0));
  EXPECT_THAT(
      user_data_.selected_address("billing_address")->Compare(*billing_address),
      Eq(0));

  // Credit card without billing address is invalid.
  credit_card->set_billing_address_id("");
  EXPECT_CALL(mock_execution_delegate_,
              NotifyUserDataChange(UserDataFieldChange::BILLING_ADDRESS));
  EXPECT_CALL(mock_execution_delegate_,
              NotifyUserDataChange(UserDataFieldChange::CARD));
  ui_controller_->HandleCreditCardChange(
      std::make_unique<autofill::CreditCard>(*credit_card),
      /* billing_profile= */ nullptr, UNKNOWN);
  EXPECT_CALL(mock_observer_, OnUserActionsChanged(UnorderedElementsAre(
                                  Property(&UserAction::enabled, Eq(false)))));
  ui_controller_->OnUserDataChanged(user_data_, UserDataFieldChange::CARD);

  EXPECT_THAT(user_data_.selected_card()->Compare(*credit_card), Eq(0));
  EXPECT_THAT(user_data_.selected_address("billing_address"), Eq(nullptr));
}

TEST_F(UiControllerTest, UserDataChangesByOutOfLoopWrite) {
  auto options = std::make_unique<FakeCollectUserDataOptions>();
  auto user_data = std::make_unique<UserData>();

  options->required_contact_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::NAME_FULL));
  options->required_contact_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::EMAIL_ADDRESS));
  options->required_contact_data_pieces.push_back(MakeRequiredDataPiece(
      autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER));
  options->contact_details_name = "selected_profile";

  testing::InSequence sequence;
  EXPECT_CALL(mock_execution_delegate_,
              NotifyUserDataChange(UserDataFieldChange::ALL));
  ui_controller_->SetCollectUserDataOptions(options.get());
  EXPECT_CALL(mock_observer_, OnUserActionsChanged(UnorderedElementsAre(
                                  Property(&UserAction::enabled, Eq(false)))));
  ui_controller_->OnUserDataChanged(user_data_, UserDataFieldChange::ALL);

  EXPECT_CALL(mock_execution_delegate_,
              NotifyUserDataChange(UserDataFieldChange::CONTACT_PROFILE));
  autofill::AutofillProfile contact_profile;
  contact_profile.SetRawInfo(autofill::ServerFieldType::EMAIL_ADDRESS,
                             u"joedoe@example.com");
  contact_profile.SetRawInfo(autofill::ServerFieldType::NAME_FULL, u"Joe Doe");
  contact_profile.SetRawInfo(autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER,
                             u"+1 23 456 789 01");
  ui_controller_->HandleContactInfoChange(
      std::make_unique<autofill::AutofillProfile>(contact_profile), UNKNOWN);

  EXPECT_CALL(mock_observer_, OnUserActionsChanged(UnorderedElementsAre(
                                  Property(&UserAction::enabled, Eq(true)))));
  ui_controller_->OnUserDataChanged(user_data_, UserDataFieldChange::ALL);

  EXPECT_THAT(
      user_data_.selected_address("selected_profile")->Compare(contact_profile),
      Eq(0));

  EXPECT_CALL(mock_observer_, OnUserActionsChanged(UnorderedElementsAre(
                                  Property(&UserAction::enabled, Eq(false)))));
  // Can be called by a PDM update.
  user_model_.SetSelectedAutofillProfile("selected_profile", nullptr,
                                         &user_data_);
  ui_controller_->OnUserDataChanged(user_data_,
                                    UserDataFieldChange::CONTACT_PROFILE);
}

TEST_F(UiControllerTest, UserDataFormReloadFromContactChange) {
  auto options = std::make_unique<FakeCollectUserDataOptions>();
  base::MockCallback<base::OnceCallback<void(UserData*)>> reload_callback;
  options->reload_data_callback = reload_callback.Get();
  base::MockCallback<
      base::RepeatingCallback<void(UserDataEventField, UserDataEventType)>>
      change_callback;
  options->selected_user_data_changed_callback = change_callback.Get();
  options->use_gms_core_edit_dialogs = true;

  ui_controller_->SetCollectUserDataOptions(options.get());

  EXPECT_CALL(change_callback, Run(UserDataEventField::CONTACT_EVENT,
                                   UserDataEventType::ENTRY_CREATED));
  EXPECT_CALL(reload_callback, Run);
  ui_controller_->HandleContactInfoChange(nullptr,
                                          UserDataEventType::ENTRY_CREATED);
}

TEST_F(UiControllerTest, UserDataFormDoNotReloadFromContactSelectionChange) {
  auto options = std::make_unique<FakeCollectUserDataOptions>();
  options->contact_details_name = "CONTACT";
  base::MockCallback<base::OnceCallback<void(UserData*)>> reload_callback;
  options->reload_data_callback = reload_callback.Get();
  base::MockCallback<
      base::RepeatingCallback<void(UserDataEventField, UserDataEventType)>>
      change_callback;
  options->selected_user_data_changed_callback = change_callback.Get();
  options->use_gms_core_edit_dialogs = true;

  ui_controller_->SetCollectUserDataOptions(options.get());

  EXPECT_CALL(change_callback, Run(UserDataEventField::CONTACT_EVENT,
                                   UserDataEventType::SELECTION_CHANGED));
  EXPECT_CALL(reload_callback, Run).Times(0);
  ui_controller_->HandleContactInfoChange(nullptr,
                                          UserDataEventType::SELECTION_CHANGED);
}

TEST_F(UiControllerTest, UserDataFormReloadFromPhoneNumberChange) {
  auto options = std::make_unique<FakeCollectUserDataOptions>();
  base::MockCallback<base::OnceCallback<void(UserData*)>> reload_callback;
  options->reload_data_callback = reload_callback.Get();
  base::MockCallback<
      base::RepeatingCallback<void(UserDataEventField, UserDataEventType)>>
      change_callback;
  options->selected_user_data_changed_callback = change_callback.Get();
  options->use_gms_core_edit_dialogs = true;

  ui_controller_->SetCollectUserDataOptions(options.get());

  EXPECT_CALL(change_callback, Run(UserDataEventField::CONTACT_EVENT,
                                   UserDataEventType::ENTRY_CREATED));
  EXPECT_CALL(reload_callback, Run);
  ui_controller_->HandlePhoneNumberChange(nullptr,
                                          UserDataEventType::ENTRY_CREATED);
}

TEST_F(UiControllerTest, UserDataFormReloadFromShippingAddressChange) {
  auto options = std::make_unique<FakeCollectUserDataOptions>();
  base::MockCallback<base::OnceCallback<void(UserData*)>> reload_callback;
  options->reload_data_callback = reload_callback.Get();
  base::MockCallback<
      base::RepeatingCallback<void(UserDataEventField, UserDataEventType)>>
      change_callback;
  options->selected_user_data_changed_callback = change_callback.Get();
  options->use_gms_core_edit_dialogs = true;

  ui_controller_->SetCollectUserDataOptions(options.get());

  EXPECT_CALL(change_callback, Run(UserDataEventField::SHIPPING_EVENT,
                                   UserDataEventType::ENTRY_CREATED));
  EXPECT_CALL(reload_callback, Run);
  ui_controller_->HandleShippingAddressChange(nullptr,
                                              UserDataEventType::ENTRY_CREATED);
}

TEST_F(UiControllerTest, UserDataFormReloadFromCreditCardChange) {
  auto options = std::make_unique<FakeCollectUserDataOptions>();
  base::MockCallback<base::OnceCallback<void(UserData*)>> reload_callback;
  options->reload_data_callback = reload_callback.Get();
  base::MockCallback<
      base::RepeatingCallback<void(UserDataEventField, UserDataEventType)>>
      change_callback;
  options->selected_user_data_changed_callback = change_callback.Get();
  options->use_gms_core_edit_dialogs = true;

  ui_controller_->SetCollectUserDataOptions(options.get());

  EXPECT_CALL(change_callback, Run(UserDataEventField::CREDIT_CARD_EVENT,
                                   UserDataEventType::ENTRY_CREATED));
  EXPECT_CALL(reload_callback, Run);
  ui_controller_->HandleCreditCardChange(nullptr, nullptr,
                                         UserDataEventType::ENTRY_CREATED);
}

TEST_F(UiControllerTest, SetTermsAndConditions) {
  auto options = std::make_unique<FakeCollectUserDataOptions>();

  options->accept_terms_and_conditions_text.assign("Accept T&C");
  testing::InSequence seq;
  EXPECT_CALL(mock_execution_delegate_,
              NotifyUserDataChange(UserDataFieldChange::ALL));
  ui_controller_->SetCollectUserDataOptions(options.get());
  EXPECT_CALL(mock_observer_, OnUserActionsChanged(UnorderedElementsAre(
                                  Property(&UserAction::enabled, Eq(false)))));
  ui_controller_->OnUserDataChanged(user_data_, UserDataFieldChange::ALL);

  EXPECT_CALL(mock_execution_delegate_,
              NotifyUserDataChange(UserDataFieldChange::TERMS_AND_CONDITIONS));
  ui_controller_->SetTermsAndConditions(TermsAndConditionsState::ACCEPTED);
  EXPECT_CALL(mock_observer_, OnUserActionsChanged(UnorderedElementsAre(
                                  Property(&UserAction::enabled, Eq(true)))));
  ui_controller_->OnUserDataChanged(user_data_,
                                    UserDataFieldChange::TERMS_AND_CONDITIONS);

  EXPECT_THAT(user_data_.terms_and_conditions_,
              Eq(TermsAndConditionsState::ACCEPTED));
}

TEST_F(UiControllerTest, SetLoginOption) {
  auto options = std::make_unique<FakeCollectUserDataOptions>();
  options->request_login_choice = true;
  LoginChoice login_choice;
  login_choice.identifier = "guest";
  options->login_choices.push_back(login_choice);

  testing::InSequence seq;
  EXPECT_CALL(mock_execution_delegate_,
              NotifyUserDataChange(UserDataFieldChange::ALL));
  ui_controller_->SetCollectUserDataOptions(options.get());
  EXPECT_CALL(mock_observer_, OnUserActionsChanged(UnorderedElementsAre(
                                  Property(&UserAction::enabled, Eq(false)))));
  ui_controller_->OnUserDataChanged(user_data_, UserDataFieldChange::ALL);

  EXPECT_CALL(mock_execution_delegate_,
              NotifyUserDataChange(UserDataFieldChange::LOGIN_CHOICE));
  ui_controller_->SetLoginOption("guest");
  EXPECT_CALL(mock_observer_, OnUserActionsChanged(UnorderedElementsAre(
                                  Property(&UserAction::enabled, Eq(true)))));
  ui_controller_->OnUserDataChanged(user_data_,
                                    UserDataFieldChange::LOGIN_CHOICE);

  EXPECT_THAT(user_data_.selected_login_choice()->identifier, Eq("guest"));
}

TEST_F(UiControllerTest, SetShippingAddress) {
  auto options = std::make_unique<FakeCollectUserDataOptions>();

  options->request_shipping = true;
  options->shipping_address_name = "shipping_address";
  testing::InSequence seq;
  EXPECT_CALL(mock_execution_delegate_,
              NotifyUserDataChange(UserDataFieldChange::ALL));
  ui_controller_->SetCollectUserDataOptions(options.get());
  EXPECT_CALL(mock_observer_, OnUserActionsChanged(UnorderedElementsAre(
                                  Property(&UserAction::enabled, Eq(false)))));
  ui_controller_->OnUserDataChanged(user_data_, UserDataFieldChange::ALL);

  auto shipping_address = std::make_unique<autofill::AutofillProfile>(
      base::GenerateGUID(), "https://www.example.com");
  autofill::test::SetProfileInfo(shipping_address.get(), "Marion", "Mitchell",
                                 "Morrison", "marion@me.xyz", "Fox",
                                 "123 Zoo St.", "unit 5", "Hollywood", "CA",
                                 "91601", "US", "16505678910");
  EXPECT_CALL(mock_execution_delegate_,
              NotifyUserDataChange(UserDataFieldChange::SHIPPING_ADDRESS));
  ui_controller_->HandleShippingAddressChange(
      std::make_unique<autofill::AutofillProfile>(*shipping_address), UNKNOWN);

  EXPECT_CALL(mock_observer_, OnUserActionsChanged(UnorderedElementsAre(
                                  Property(&UserAction::enabled, Eq(true)))));
  ui_controller_->OnUserDataChanged(user_data_, UserDataFieldChange::ALL);

  EXPECT_THAT(user_data_.selected_address("shipping_address")
                  ->Compare(*shipping_address),
              Eq(0));
}

TEST_F(UiControllerTest, SetAdditionalValues) {
  auto options = std::make_unique<FakeCollectUserDataOptions>();
  ValueProto value1;
  value1.mutable_strings()->add_values("123456789");

  ValueProto value2;
  value2.mutable_strings()->add_values("");
  ValueProto value3;
  value3.mutable_strings()->add_values("");
  user_data_.SetAdditionalValue("key1", value1);
  user_data_.SetAdditionalValue("key2", value2);
  user_data_.SetAdditionalValue("key3", value3);

  ui_controller_->OnUserDataChanged(user_data_,
                                    UserDataFieldChange::ADDITIONAL_VALUES);

  testing::InSequence seq;
  EXPECT_CALL(mock_execution_delegate_,
              NotifyUserDataChange(UserDataFieldChange::ALL));
  ui_controller_->SetCollectUserDataOptions(options.get());
  EXPECT_CALL(mock_observer_, OnUserActionsChanged(UnorderedElementsAre(
                                  Property(&UserAction::enabled, Eq(true)))));
  ui_controller_->OnUserDataChanged(user_data_, UserDataFieldChange::ALL);

  for (int i = 0; i < 2; ++i) {
    EXPECT_CALL(mock_execution_delegate_,
                NotifyUserDataChange(UserDataFieldChange::ADDITIONAL_VALUES));
  }
  ValueProto value4;
  value4.mutable_strings()->add_values("value2");
  ValueProto value5;
  value5.mutable_strings()->add_values("value3");
  ui_controller_->SetAdditionalValue("key2", value4);
  ui_controller_->SetAdditionalValue("key3", value5);
  EXPECT_EQ(*user_data_.GetAdditionalValue("key1"), value1);
  EXPECT_EQ(*user_data_.GetAdditionalValue("key2"), value4);
  EXPECT_EQ(*user_data_.GetAdditionalValue("key3"), value5);

  ValueProto value6;
  value6.mutable_strings()->add_values("someValue");
  EXPECT_DCHECK_DEATH(ui_controller_->SetAdditionalValue("key4", value6));
}

TEST_F(UiControllerTest, EnableTts) {
  EXPECT_CALL(mock_client_, IsSpokenFeedbackAccessibilityServiceEnabled())
      .WillOnce(Return(false));
  EXPECT_CALL(mock_observer_, OnTtsButtonVisibilityChanged(true));

  TriggerContext trigger_context(
      /* parameters = */ std::make_unique<ScriptParameters>(
          base::flat_map<std::string, std::string>{{"ENABLE_TTS", "true"}}),
      TriggerContext::Options());
  ui_controller_->OnStart(trigger_context);

  EXPECT_TRUE(ui_controller_->GetTtsButtonVisible());
}

TEST_F(UiControllerTest, DoNotEnableTtsWhenAccessibilityEnabled) {
  EXPECT_CALL(mock_client_, IsSpokenFeedbackAccessibilityServiceEnabled())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_observer_, OnTtsButtonVisibilityChanged(true)).Times(0);

  TriggerContext trigger_context(
      /* parameters = */ std::make_unique<ScriptParameters>(
          base::flat_map<std::string, std::string>{{"ENABLE_TTS", "true"}}),
      TriggerContext::Options());
  ui_controller_->OnStart(trigger_context);

  EXPECT_FALSE(ui_controller_->GetTtsButtonVisible());
}

TEST_F(UiControllerTest, TtsMessageIsSetCorrectlyAtStartup) {
  ui_controller_->OnStart(trigger_context_);
  EXPECT_EQ(ui_controller_->GetTtsMessage(),
            ui_controller_->GetStatusMessage());
  EXPECT_FALSE(ui_controller_->GetTtsMessage().empty());
}

TEST_F(UiControllerTest, TtsMessageIsSetCorrectly) {
  // SetStatusMessage should override tts_message
  ui_controller_->SetStatusMessage("message");
  EXPECT_EQ(ui_controller_->GetTtsMessage(), "message");

  ui_controller_->SetTtsMessage("tts_message");
  EXPECT_EQ(ui_controller_->GetTtsMessage(), "tts_message");
  EXPECT_EQ(ui_controller_->GetStatusMessage(), "message");
}

TEST_F(UiControllerTest, SetTtsMessageStopsAnyOngoingTts) {
  EnableTtsForTest();
  SetTtsButtonStateForTest(TtsButtonState::PLAYING);

  EXPECT_CALL(*mock_tts_controller_, Stop());
  EXPECT_CALL(mock_observer_, OnTtsButtonStateChanged(TtsButtonState::DEFAULT));
  ui_controller_->SetTtsMessage("tts_message");
  EXPECT_EQ(ui_controller_->GetTtsButtonState(), TtsButtonState::DEFAULT);
}

TEST_F(UiControllerTest, SetTtsMessageReEnablesTtsButtonWithNonStickyStateExp) {
  EXPECT_CALL(mock_client_, IsSpokenFeedbackAccessibilityServiceEnabled())
      .WillOnce(Return(false));

  TriggerContext trigger_context(
      /* parameters = */ std::make_unique<ScriptParameters>(
          base::flat_map<std::string, std::string>{{"ENABLE_TTS", "true"}}),
      TriggerContext::Options(
          /* experiment_ids= */ "4624822", /* is_cct= */ false,
          /* onboarding_shown= */ false, /* is_direct_action= */ false,
          /* initial_url= */ "http://a.example.com/path",
          /* is_in_chrome_triggered= */ false));
  EXPECT_CALL(mock_execution_delegate_, GetTriggerContext())
      .WillRepeatedly(Return(&trigger_context));
  ui_controller_->OnStart(trigger_context);
  SetTtsButtonStateForTest(TtsButtonState::DISABLED);

  EXPECT_CALL(mock_observer_, OnTtsButtonStateChanged(TtsButtonState::DEFAULT));
  ui_controller_->SetTtsMessage("tts_message");
  EXPECT_EQ(ui_controller_->GetTtsButtonState(), TtsButtonState::DEFAULT);
}

TEST_F(UiControllerTest,
       SetTtsMessageKeepsTtsButtonDisabledWithoutNonStickyStateExp) {
  EXPECT_CALL(mock_client_, IsSpokenFeedbackAccessibilityServiceEnabled())
      .WillOnce(Return(false));

  TriggerContext trigger_context(
      /* parameters = */ std::make_unique<ScriptParameters>(
          base::flat_map<std::string, std::string>{{"ENABLE_TTS", "true"}}),
      TriggerContext::Options());
  EXPECT_CALL(mock_execution_delegate_, GetTriggerContext())
      .WillRepeatedly(Return(&trigger_context));
  ui_controller_->OnStart(trigger_context);
  SetTtsButtonStateForTest(TtsButtonState::DISABLED);

  EXPECT_CALL(mock_observer_, OnTtsButtonStateChanged(_)).Times(0);
  ui_controller_->SetTtsMessage("tts_message");
  EXPECT_EQ(ui_controller_->GetTtsButtonState(), TtsButtonState::DISABLED);
}

TEST_F(UiControllerTest, TappingTtsButtonInDefaultStateStartsPlayingTts) {
  EnableTtsForTest();
  SetTtsButtonStateForTest(TtsButtonState::DEFAULT);
  ui_controller_->SetTtsMessage("tts_message");

  EXPECT_CALL(*mock_tts_controller_, Speak("tts_message", kClientLocale));
  ui_controller_->OnTtsButtonClicked();
}

TEST_F(UiControllerTest, TappingTtsButtonWhilePlayingDisablesTtsButton) {
  EnableTtsForTest();
  SetTtsButtonStateForTest(TtsButtonState::PLAYING);

  EXPECT_CALL(mock_observer_,
              OnTtsButtonStateChanged(TtsButtonState::DISABLED));
  EXPECT_CALL(*mock_tts_controller_, Stop());
  ui_controller_->OnTtsButtonClicked();
  EXPECT_EQ(ui_controller_->GetTtsButtonState(), TtsButtonState::DISABLED);
}

TEST_F(UiControllerTest, TappingDisabledTtsButtonReEnablesItAndStartsTts) {
  EnableTtsForTest();
  SetTtsButtonStateForTest(TtsButtonState::DISABLED);
  ui_controller_->SetTtsMessage("tts_message");

  EXPECT_CALL(mock_observer_, OnTtsButtonStateChanged(TtsButtonState::DEFAULT));
  EXPECT_CALL(*mock_tts_controller_, Speak("tts_message", kClientLocale));
  ui_controller_->OnTtsButtonClicked();
  EXPECT_EQ(ui_controller_->GetTtsButtonState(), TtsButtonState::DEFAULT);
}

TEST_F(UiControllerTest, MaybePlayTtsMessageDoesNotStartTtsIfTtsNotEnabled) {
  // tts_enabled_ is false by default
  ui_controller_->SetTtsMessage("tts_message");

  EXPECT_CALL(*mock_tts_controller_, Speak("tts_message", kClientLocale))
      .Times(0);
  ui_controller_->MaybePlayTtsMessage();
}

TEST_F(UiControllerTest, MaybePlayTtsMessageStartsPlayingCorrectTtsMessage) {
  EnableTtsForTest();
  ui_controller_->SetStatusMessage("message");
  ui_controller_->SetTtsMessage("tts_message");

  EXPECT_CALL(*mock_tts_controller_, Speak("tts_message", kClientLocale));
  ui_controller_->MaybePlayTtsMessage();

  // Change display strings locale.
  ClientSettingsProto client_settings_proto;
  client_settings_proto.set_display_strings_locale("test-locale");
  client_settings_.UpdateFromProto(client_settings_proto);
  EXPECT_CALL(*mock_tts_controller_, Speak("tts_message", "test-locale"));
  ui_controller_->MaybePlayTtsMessage();
}

TEST_F(UiControllerTest, OnTtsEventChangesTtsButtonStateCorrectly) {
  EXPECT_EQ(ui_controller_->GetTtsButtonState(), TtsButtonState::DEFAULT);

  EXPECT_CALL(mock_observer_, OnTtsButtonStateChanged(TtsButtonState::PLAYING));
  ui_controller_->OnTtsEvent(AutofillAssistantTtsController::TTS_START);
  EXPECT_EQ(ui_controller_->GetTtsButtonState(), TtsButtonState::PLAYING);

  EXPECT_CALL(mock_observer_, OnTtsButtonStateChanged(TtsButtonState::DEFAULT));
  ui_controller_->OnTtsEvent(AutofillAssistantTtsController::TTS_END);
  EXPECT_EQ(ui_controller_->GetTtsButtonState(), TtsButtonState::DEFAULT);

  EXPECT_CALL(mock_observer_, OnTtsButtonStateChanged(TtsButtonState::DEFAULT));
  ui_controller_->OnTtsEvent(AutofillAssistantTtsController::TTS_ERROR);
  EXPECT_EQ(ui_controller_->GetTtsButtonState(), TtsButtonState::DEFAULT);
}

TEST_F(UiControllerTest, EnablingAccessibilityStopsTtsAndHidesTtsButton) {
  EnableTtsForTest();
  SetTtsButtonStateForTest(TtsButtonState::PLAYING);

  EXPECT_CALL(*mock_tts_controller_, Stop());
  EXPECT_CALL(mock_observer_, OnTtsButtonStateChanged(TtsButtonState::DEFAULT));
  EXPECT_CALL(mock_observer_,
              OnTtsButtonVisibilityChanged(/* visibility= */ false));
  ui_controller_->OnSpokenFeedbackAccessibilityServiceChanged(
      /* enabled= */ true);
  EXPECT_FALSE(ui_controller_->GetTtsButtonVisible());
  EXPECT_EQ(ui_controller_->GetTtsButtonState(), TtsButtonState::DEFAULT);
}

TEST_F(UiControllerTest, DisablingAccessibilityShouldNotEnableTts) {
  // TTS is disabled by default.
  EXPECT_FALSE(ui_controller_->GetTtsButtonVisible());

  EXPECT_CALL(mock_observer_,
              OnTtsButtonVisibilityChanged(/* visibility= */ false))
      .Times(0);
  ui_controller_->OnSpokenFeedbackAccessibilityServiceChanged(
      /* enabled= */ false);
  EXPECT_FALSE(ui_controller_->GetTtsButtonVisible());
}

TEST_F(UiControllerTest, HidingUiStopsAnyOngoingTts) {
  EnableTtsForTest();
  SetTtsButtonStateForTest(TtsButtonState::PLAYING);

  EXPECT_CALL(*mock_tts_controller_, Stop());
  EXPECT_CALL(mock_observer_, OnTtsButtonStateChanged(TtsButtonState::DEFAULT));
  ui_controller_->OnUiShownChanged(/* shown= */ false);
  EXPECT_EQ(ui_controller_->GetTtsButtonState(), TtsButtonState::DEFAULT);
}

TEST_F(UiControllerTest, ExpandOrCollapseBottomSheet) {
  {
    testing::InSequence seq;
    EXPECT_CALL(mock_observer_, OnCollapseBottomSheet());
    EXPECT_CALL(mock_observer_, OnExpandBottomSheet());
  }
  ui_controller_->CollapseBottomSheet();
  ui_controller_->ExpandBottomSheet();
}

TEST_F(UiControllerTest, ShouldPromptActionExpandSheet) {
  // Expect this to be true initially.
  EXPECT_TRUE(ui_controller_->ShouldPromptActionExpandSheet());

  ui_controller_->SetExpandSheetForPromptAction(false);
  EXPECT_FALSE(ui_controller_->ShouldPromptActionExpandSheet());

  ui_controller_->SetExpandSheetForPromptAction(true);
  EXPECT_TRUE(ui_controller_->ShouldPromptActionExpandSheet());
}

TEST_F(UiControllerTest, SetGenericUi) {
  {
    testing::InSequence seq;
    EXPECT_CALL(mock_observer_, OnGenericUserInterfaceChanged(NotNull()));
    EXPECT_CALL(mock_observer_, OnGenericUserInterfaceChanged(nullptr));
  }
  ui_controller_->SetGenericUi(
      std::make_unique<GenericUserInterfaceProto>(GenericUserInterfaceProto()),
      base::DoNothing(), base::DoNothing());
  ui_controller_->ClearGenericUi();
}

TEST_F(UiControllerTest, OnShowFirstMessageShowsDefaultInitialStatusMessage) {
  EXPECT_CALL(mock_observer_,
              OnStatusMessageChanged(l10n_util::GetStringFUTF8(
                  IDS_AUTOFILL_ASSISTANT_LOADING, u"www.example.com")));
  ui_controller_->OnStart(trigger_context_);
}

TEST_F(UiControllerTest, NotifyObserversOfInitialStatusMessageAndProgressBar) {
  ShowProgressBarProto::StepProgressBarConfiguration progress_bar_configuration;
  progress_bar_configuration.add_annotated_step_icons()
      ->mutable_icon()
      ->set_icon(DrawableProto::PROGRESSBAR_DEFAULT_INITIAL_STEP);
  progress_bar_configuration.add_annotated_step_icons()
      ->mutable_icon()
      ->set_icon(DrawableProto::PROGRESSBAR_DEFAULT_DATA_COLLECTION);
  progress_bar_configuration.add_annotated_step_icons()
      ->mutable_icon()
      ->set_icon(DrawableProto::PROGRESSBAR_DEFAULT_PAYMENT);
  progress_bar_configuration.add_annotated_step_icons()
      ->mutable_icon()
      ->set_icon(DrawableProto::PROGRESSBAR_DEFAULT_FINAL_STEP);

  // When setting UI state of the controller before calling |Start|, observers
  // will be notified immediately after |Start|.
  ui_controller_->SetStatusMessage("startup message");
  ui_controller_->SetStepProgressBarConfiguration(progress_bar_configuration);
  ui_controller_->SetProgressActiveStep(1);

  EXPECT_CALL(mock_observer_, OnStepProgressBarConfigurationChanged(
                                  progress_bar_configuration));
  EXPECT_CALL(mock_observer_, OnProgressActiveStepChanged(1));
  EXPECT_CALL(mock_observer_, OnStatusMessageChanged("startup message"));
  ui_controller_->OnStart(trigger_context_);
}

TEST_F(UiControllerTest, Details) {
  // The current controller details, as notified to the observers.
  std::vector<Details> observed_details;

  ON_CALL(mock_observer_, OnDetailsChanged(_))
      .WillByDefault(
          Invoke([&observed_details](const std::vector<Details>& details) {
            observed_details = details;
          }));

  // Details are initially empty.
  EXPECT_THAT(ui_controller_->GetDetails(), IsEmpty());

  // Set 2 details.
  ui_controller_->SetDetails(std::make_unique<Details>(), base::TimeDelta());
  EXPECT_THAT(ui_controller_->GetDetails(), SizeIs(1));
  EXPECT_THAT(observed_details, SizeIs(1));

  // Set 2 details in 1s (which directly clears the current details).
  ui_controller_->SetDetails(std::make_unique<Details>(),
                             base::Milliseconds(1000));
  EXPECT_THAT(ui_controller_->GetDetails(), IsEmpty());
  EXPECT_THAT(observed_details, IsEmpty());

  task_environment_.FastForwardBy(base::Milliseconds(1000));
  EXPECT_THAT(ui_controller_->GetDetails(), SizeIs(1));
  EXPECT_THAT(observed_details, SizeIs(1));

  ui_controller_->AppendDetails(std::make_unique<Details>(),
                                /* delay= */ base::TimeDelta());
  EXPECT_THAT(ui_controller_->GetDetails(), SizeIs(2));
  EXPECT_THAT(observed_details, SizeIs(2));

  // Delay the appending of the details.
  ui_controller_->AppendDetails(std::make_unique<Details>(),
                                /* delay= */ base::Milliseconds(1000));
  EXPECT_THAT(ui_controller_->GetDetails(), SizeIs(2));
  EXPECT_THAT(observed_details, SizeIs(2));

  task_environment_.FastForwardBy(base::Milliseconds(999));
  EXPECT_THAT(ui_controller_->GetDetails(), SizeIs(2));
  EXPECT_THAT(observed_details, SizeIs(2));

  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_THAT(ui_controller_->GetDetails(), SizeIs(3));
  EXPECT_THAT(observed_details, SizeIs(3));

  // Setting the details clears the timers.
  ui_controller_->AppendDetails(std::make_unique<Details>(),
                                /* delay= */ base::Milliseconds(1000));
  ui_controller_->SetDetails(nullptr, base::TimeDelta());
  EXPECT_THAT(ui_controller_->GetDetails(), IsEmpty());
  EXPECT_THAT(observed_details, IsEmpty());

  task_environment_.FastForwardBy(base::Milliseconds(2000));
  EXPECT_THAT(ui_controller_->GetDetails(), IsEmpty());
  EXPECT_THAT(observed_details, IsEmpty());
}

TEST_F(UiControllerTest, OnScriptErrorWillAppendVanishingFeedbackChip) {
  // A script error should show the feedback chip.
  EXPECT_CALL(mock_observer_, OnUserActionsChanged(SizeIs(1)));
  ui_controller_->OnError("Error", Metrics::DropOutReason::NAVIGATION);
  ui_controller_->OnStop();

  // The chip should vanish once clicked.
  EXPECT_CALL(mock_observer_, OnUserActionsChanged(SizeIs(0)));
  EXPECT_CALL(mock_execution_delegate_, ShutdownIfNecessary());
  ui_controller_->PerformUserAction(0);
}

// The chip should be hidden if and only if the keyboard is visible and the
// focus is on a bottom sheet input text.
TEST_F(UiControllerTest, UpdateChipVisibility) {
  InSequence seq;

  UserAction user_action(ChipProto(), true, std::string());
  EXPECT_CALL(mock_observer_,
              OnUserActionsChanged(UnorderedElementsAre(Property(
                  &UserAction::chip, Field(&Chip::visible, Eq(true))))));
  auto user_actions = std::make_unique<std::vector<UserAction>>();
  user_actions->emplace_back(std::move(user_action));
  ui_controller_->SetUserActions(std::move(user_actions));

  EXPECT_CALL(mock_observer_, OnUserActionsChanged(_)).Times(0);
  ui_controller_->OnKeyboardVisibilityChanged(true);

  EXPECT_CALL(mock_observer_,
              OnUserActionsChanged(UnorderedElementsAre(Property(
                  &UserAction::chip, Field(&Chip::visible, Eq(false))))));
  ui_controller_->OnInputTextFocusChanged(true);

  EXPECT_CALL(mock_observer_,
              OnUserActionsChanged(UnorderedElementsAre(Property(
                  &UserAction::chip, Field(&Chip::visible, Eq(true))))));
  ui_controller_->OnKeyboardVisibilityChanged(false);

  EXPECT_CALL(mock_observer_, OnUserActionsChanged(_)).Times(0);
  ui_controller_->OnInputTextFocusChanged(false);
}

TEST_F(UiControllerTest, UpdateUserActionsOnUserDataChanged) {
  // Note that the UiController ignores both of the arguments of the
  // OnUserDataChanged notification.

  auto actions = std::make_unique<std::vector<UserAction>>();
  actions->push_back(MakeUserAction("Continue"));
  actions->push_back(MakeUserAction("Cancel"));
  EXPECT_CALL(mock_observer_, OnUserActionsChanged(SizeIs(2)));
  ui_controller_->SetUserActions(std::move(actions));

  // If no CollectUserDataOptions are specified, the user actions are cleared.
  EXPECT_CALL(mock_observer_, OnUserActionsChanged(SizeIs(0)));
  ui_controller_->OnUserDataChanged(user_data_, UserDataFieldChange::ALL);
}

TEST_F(UiControllerTest, OnExecuteScriptSetMessageAndClearUserActions) {
  ui_controller_->SetStatusMessage("initial message");
  EXPECT_EQ(ui_controller_->GetStatusMessage(), "initial message");

  EXPECT_CALL(mock_observer_, OnStatusMessageChanged("script message"));
  EXPECT_CALL(mock_observer_, OnUserActionsChanged(SizeIs(0)));
  ui_controller_->OnExecuteScript("script message");
  EXPECT_EQ(ui_controller_->GetStatusMessage(), "script message");

  // If the message is empty, the status message is not updated.
  EXPECT_CALL(mock_observer_, OnStatusMessageChanged(_)).Times(0);
  EXPECT_CALL(mock_observer_, OnUserActionsChanged(SizeIs(0)));
  ui_controller_->OnExecuteScript("");
  // The message should still be the last one set before this call.
  EXPECT_EQ(ui_controller_->GetStatusMessage(), "script message");
}

TEST_F(UiControllerTest, SetCollectUserDataUiState) {
  EXPECT_CALL(mock_observer_,
              OnCollectUserDataUiStateChanged(/* enabled= */ false));
  ui_controller_->SetCollectUserDataUiState(false);
}

}  // namespace autofill_assistant
