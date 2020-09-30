// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/collect_user_data_action.h"

#include <utility>

#include "base/guid.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/mock_personal_data_manager.h"
#include "components/autofill_assistant/browser/mock_website_login_manager.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
const char kFakeUrl[] = "https://www.example.com";
const char kFakeUsername[] = "user@example.com";
const char kFakePassword[] = "example_password";

const char kMemoryLocation[] = "billing";
}  // namespace

namespace autofill_assistant {
namespace {

void SetDateProto(DateProto* proto, int year, int month, int day) {
  proto->set_year(year);
  proto->set_month(month);
  proto->set_day(day);
}

MATCHER_P(EqualsProto, message, "") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::IsSupersetOf;
using ::testing::Property;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

class CollectUserDataActionTest : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    ON_CALL(mock_action_delegate_, GetPersonalDataManager)
        .WillByDefault(Return(&mock_personal_data_manager_));
    ON_CALL(mock_action_delegate_, GetWebsiteLoginManager)
        .WillByDefault(Return(&mock_website_login_manager_));
    ON_CALL(mock_action_delegate_, GetUserData)
        .WillByDefault(Return(&user_data_));
    ON_CALL(mock_action_delegate_, WriteUserData(_))
        .WillByDefault(Invoke(
            [this](base::OnceCallback<void(UserData*, UserData::FieldChange*)>
                       write_callback) {
              UserData::FieldChange field_change = UserData::FieldChange::NONE;
              std::move(write_callback).Run(&user_data_, &field_change);
            }));
    ON_CALL(mock_action_delegate_, CollectUserData(_))
        .WillByDefault(
            Invoke([this](CollectUserDataOptions* collect_user_data_options) {
              std::move(collect_user_data_options->confirm_callback)
                  .Run(&user_data_, &user_model_);
            }));

    ON_CALL(mock_website_login_manager_, OnGetLoginsForUrl(_, _))
        .WillByDefault(
            RunOnceCallback<1>(std::vector<WebsiteLoginManager::Login>{
                WebsiteLoginManager::Login(GURL(kFakeUrl), kFakeUsername)}));
    ON_CALL(mock_website_login_manager_, OnGetPasswordForLogin(_, _))
        .WillByDefault(RunOnceCallback<1>(true, kFakePassword));

    content::WebContentsTester::For(web_contents())
        ->SetLastCommittedURL(GURL(kFakeUrl));
    ON_CALL(mock_action_delegate_, GetWebContents())
        .WillByDefault(Return(web_contents()));
  }

 protected:
  base::MockCallback<Action::ProcessActionCallback> callback_;
  MockPersonalDataManager mock_personal_data_manager_;
  MockWebsiteLoginManager mock_website_login_manager_;
  MockActionDelegate mock_action_delegate_;
  UserData user_data_;
  UserModel user_model_;
};

TEST_F(CollectUserDataActionTest, FailsForMissingPrivacyText) {
  ActionProto action_proto;
  action_proto.mutable_collect_user_data();

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, SucceedsForPrivacyTextPresent) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_privacy_notice_text("privacy");
  collect_user_data_proto->set_request_terms_and_conditions(false);

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            user_data_.terms_and_conditions_ = ACCEPTED;
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::collect_user_data_result,
              Property(
                  &CollectUserDataResultProto::is_terms_and_conditions_accepted,
                  true))))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, FailsForMissingTermsAcceptTextIfRequired) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_privacy_notice_text("privacy");
  collect_user_data_proto->set_request_terms_and_conditions(true);
  collect_user_data_proto->set_terms_require_review_text("terms review");

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, FailsForMissingTermsReviewTextIfRequired) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_privacy_notice_text("privacy");
  collect_user_data_proto->set_request_terms_and_conditions(true);
  collect_user_data_proto->set_accept_terms_and_conditions_text(
      "terms and conditions");
  collect_user_data_proto->set_show_terms_as_checkbox(false);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, SucceedsForCheckboxIfReviewTextMissing) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_privacy_notice_text("privacy");
  collect_user_data_proto->set_request_terms_and_conditions(true);
  collect_user_data_proto->set_accept_terms_and_conditions_text(
      "terms and conditions");
  collect_user_data_proto->set_show_terms_as_checkbox(true);

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            user_data_.terms_and_conditions_ = ACCEPTED;
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::collect_user_data_result,
              Property(
                  &CollectUserDataResultProto::is_terms_and_conditions_accepted,
                  true))))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, SucceedsForAllTermsTextPresent) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_privacy_notice_text("privacy");
  collect_user_data_proto->set_request_terms_and_conditions(true);
  collect_user_data_proto->set_accept_terms_and_conditions_text(
      "terms and conditions");
  collect_user_data_proto->set_show_terms_as_checkbox(false);
  collect_user_data_proto->set_terms_require_review_text("terms review");

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            user_data_.terms_and_conditions_ = ACCEPTED;
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::collect_user_data_result,
              Property(
                  &CollectUserDataResultProto::is_terms_and_conditions_accepted,
                  true))))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, InfoSectionText) {
  const char kInfoSection[] = "Info section.";

  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_info_section_text(kInfoSection);
  collect_user_data_proto->set_request_terms_and_conditions(false);

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));
  EXPECT_CALL(
      mock_action_delegate_,
      CollectUserData(Pointee(Field(&CollectUserDataOptions::info_section_text,
                                    StrEq(kInfoSection)))));

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, PromptIsShown) {
  const char kPrompt[] = "Some message.";

  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  collect_user_data_proto->set_prompt(kPrompt);

  EXPECT_CALL(mock_action_delegate_, SetStatusMessage(kPrompt));
  EXPECT_CALL(callback_, Run(_));

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, SelectLogin) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* login_details = collect_user_data_proto->mutable_login_details();
  auto* login_option = login_details->add_login_options();
  login_option->mutable_password_manager();
  login_option->set_payload("payload");

  // Action should fetch the logins, but not the passwords.
  EXPECT_CALL(mock_website_login_manager_, OnGetLoginsForUrl(GURL(kFakeUrl), _))
      .Times(1);
  EXPECT_CALL(mock_website_login_manager_, OnGetPasswordForLogin(_, _))
      .Times(0);

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            user_data_.login_choice_identifier_.assign(
                collect_user_data_options->login_choices[0].identifier);
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  EXPECT_CALL(callback_,
              Run(Pointee(AllOf(
                  Property(&ProcessedActionProto::status, ACTION_APPLIED),
                  Property(&ProcessedActionProto::collect_user_data_result,
                           Property(&CollectUserDataResultProto::login_payload,
                                    "payload")),
                  Property(&ProcessedActionProto::collect_user_data_result,
                           Property(&CollectUserDataResultProto::shown_to_user,
                                    true))))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, SelectLoginMissingUsername) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* login_details = collect_user_data_proto->mutable_login_details();
  auto* login_option = login_details->add_login_options();
  login_option->mutable_password_manager();
  login_option->set_payload("payload");

  ON_CALL(mock_website_login_manager_, OnGetLoginsForUrl(_, _))
      .WillByDefault(RunOnceCallback<1>(std::vector<WebsiteLoginManager::Login>{
          WebsiteLoginManager::Login(GURL(kFakeUrl), /*username=*/"")}));
  // Action should fetch the logins, but not the passwords.
  EXPECT_CALL(mock_website_login_manager_, OnGetLoginsForUrl(GURL(kFakeUrl), _))
      .Times(1);
  EXPECT_CALL(mock_website_login_manager_, OnGetPasswordForLogin(_, _))
      .Times(0);

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            user_data_.login_choice_identifier_.assign(
                collect_user_data_options->login_choices[0].identifier);
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::collect_user_data_result,
              Property(&CollectUserDataResultProto::login_payload, "payload")),
          Property(&ProcessedActionProto::collect_user_data_result,
                   Property(&CollectUserDataResultProto::shown_to_user, true)),
          Property(&ProcessedActionProto::collect_user_data_result,
                   Property(&CollectUserDataResultProto::login_missing_username,
                            true))))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, LoginChoiceAutomaticIfNoOtherOptions) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* login_details = collect_user_data_proto->mutable_login_details();
  auto* login_option = login_details->add_login_options();
  login_option->mutable_custom()->set_label("Guest Checkout");
  login_option->set_payload("guest");
  login_option->set_choose_automatically_if_no_stored_login(true);
  login_option = login_details->add_login_options();
  login_option->mutable_password_manager();
  login_option->set_payload("password_manager");

  ON_CALL(mock_website_login_manager_, OnGetLoginsForUrl(_, _))
      .WillByDefault(
          RunOnceCallback<1>(std::vector<WebsiteLoginManager::Login>{}));

  EXPECT_CALL(mock_action_delegate_, CollectUserData(_)).Times(0);
  EXPECT_CALL(callback_,
              Run(Pointee(AllOf(
                  Property(&ProcessedActionProto::status, ACTION_APPLIED),
                  Property(&ProcessedActionProto::collect_user_data_result,
                           Property(&CollectUserDataResultProto::login_payload,
                                    "guest")),
                  Property(&ProcessedActionProto::collect_user_data_result,
                           Property(&CollectUserDataResultProto::shown_to_user,
                                    false))))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest,
       LoginChoiceAutomaticIfNoPasswordManagerLogins) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* login_details = collect_user_data_proto->mutable_login_details();
  auto* login_option = login_details->add_login_options();
  login_option->mutable_custom()->set_label("Guest Checkout");
  login_option->set_payload("guest");
  login_option->set_choose_automatically_if_no_stored_login(true);

  login_option = login_details->add_login_options();
  login_option->mutable_custom()->set_label("Manual");
  login_option->set_payload("manual");
  login_option->set_choose_automatically_if_no_stored_login(false);

  login_option = login_details->add_login_options();
  login_option->mutable_password_manager();
  login_option->set_payload("password_manager");

  ON_CALL(mock_website_login_manager_, OnGetLoginsForUrl(_, _))
      .WillByDefault(
          RunOnceCallback<1>(std::vector<WebsiteLoginManager::Login>{}));

  EXPECT_CALL(mock_action_delegate_, CollectUserData(_)).Times(0);
  EXPECT_CALL(callback_,
              Run(Pointee(AllOf(
                  Property(&ProcessedActionProto::status, ACTION_APPLIED),
                  Property(&ProcessedActionProto::collect_user_data_result,
                           Property(&CollectUserDataResultProto::login_payload,
                                    "guest")),
                  Property(&ProcessedActionProto::collect_user_data_result,
                           Property(&CollectUserDataResultProto::shown_to_user,
                                    false))))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, EarlyActionReturnIfOnlyLoginRequested) {
  ON_CALL(mock_action_delegate_, CollectUserData(_)).WillByDefault(Return());

  ActionProto action_proto;
  auto* proto = action_proto.mutable_collect_user_data();
  auto* login_details = proto->mutable_login_details();
  auto* login_option = login_details->add_login_options();
  login_option->mutable_custom()->set_label("Guest Checkout");
  login_option->set_payload("guest");
  login_option->set_choose_automatically_if_no_stored_login(true);

  login_option = login_details->add_login_options();
  login_option->mutable_password_manager();
  login_option->set_payload("password_manager");

  ON_CALL(mock_website_login_manager_, OnGetLoginsForUrl(_, _))
      .WillByDefault(
          RunOnceCallback<1>(std::vector<WebsiteLoginManager::Login>{}));

  // Terms requested, no early return.
  proto->set_request_terms_and_conditions(true);
  proto->set_accept_terms_and_conditions_text("I accept");
  proto->set_show_terms_as_checkbox(true);
  {
    EXPECT_CALL(mock_action_delegate_, CollectUserData(_)).Times(1);
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  // Contact info requested, no early return.
  proto->set_request_terms_and_conditions(false);
  proto->mutable_contact_details()->set_request_payer_name(true);
  proto->mutable_contact_details()->set_contact_details_name("name");
  {
    EXPECT_CALL(mock_action_delegate_, CollectUserData(_)).Times(1);
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  // Shipping info requested, no early return.
  proto->clear_contact_details();
  proto->set_shipping_address_name("shipping");
  {
    EXPECT_CALL(mock_action_delegate_, CollectUserData(_)).Times(1);
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  // Payment info requested, no early return.
  proto->clear_shipping_address_name();
  proto->set_request_payment_method(true);
  proto->set_billing_address_name("billing");
  {
    EXPECT_CALL(mock_action_delegate_, CollectUserData(_)).Times(1);
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  // Date/time range info requested, no early return.
  proto->clear_request_payment_method();
  proto->clear_billing_address_name();
  auto* date_time_range = proto->mutable_date_time_range();
  SetDateProto(date_time_range->mutable_start_date(), 2020, 1, 1);
  SetDateProto(date_time_range->mutable_end_date(), 2020, 1, 15);
  SetDateProto(date_time_range->mutable_min_date(), 2020, 1, 1);
  SetDateProto(date_time_range->mutable_max_date(), 2020, 12, 31);
  date_time_range->set_start_time_slot(0);
  date_time_range->set_end_time_slot(0);
  date_time_range->set_start_date_label("Start date");
  date_time_range->set_end_date_label("End date");
  date_time_range->set_start_time_label("Start time");
  date_time_range->set_end_time_label("End time");
  date_time_range->set_date_not_set_error("Date not set");
  date_time_range->set_time_not_set_error("Time not set");
  auto* time_slot = date_time_range->add_time_slots();
  time_slot->set_label("08:00 AM");
  time_slot->set_comparison_value(0);
  time_slot = date_time_range->add_time_slots();
  time_slot->set_label("09:00 AM");
  time_slot->set_comparison_value(1);
  {
    EXPECT_CALL(mock_action_delegate_, CollectUserData(_)).Times(1);
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  // Generic UI model identifier set, no early return.
  proto->clear_date_time_range();
  proto->set_additional_model_identifier_to_check("identifier");
  {
    EXPECT_CALL(mock_action_delegate_, CollectUserData(_)).Times(1);
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  // Additional prepended input section, no early return.
  proto->clear_additional_model_identifier_to_check();
  auto* section = proto->add_additional_prepended_sections();
  section->set_title("Title");
  auto* input_field = section->mutable_text_input_section()->add_input_fields();
  input_field->set_hint("hint");
  input_field->set_input_type(TextInputProto::INPUT_TEXT);
  input_field->set_client_memory_key("key");
  {
    EXPECT_CALL(mock_action_delegate_, CollectUserData(_)).Times(1);
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  // Additional appended input section, no early return.
  proto->clear_additional_prepended_sections();
  section = proto->add_additional_appended_sections();
  section->set_title("title");
  auto* popup = section->mutable_popup_list_section();
  popup->set_additional_value_key("key");
  popup->add_item_names("item");
  popup->set_no_selection_error_message("error");
  {
    EXPECT_CALL(mock_action_delegate_, CollectUserData(_)).Times(1);
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  // Additional static section is allowed, expect early return.
  proto->clear_additional_appended_sections();
  section = proto->add_additional_appended_sections();
  section->set_title("title");
  section->mutable_static_text_section()->set_text("text");
  {
    EXPECT_CALL(mock_action_delegate_, CollectUserData(_)).Times(0);
    EXPECT_CALL(
        callback_,
        Run(Pointee(AllOf(
            Property(&ProcessedActionProto::status, ACTION_APPLIED),
            Property(
                &ProcessedActionProto::collect_user_data_result,
                Property(&CollectUserDataResultProto::login_payload, "guest")),
            Property(&ProcessedActionProto::collect_user_data_result,
                     Property(&CollectUserDataResultProto::shown_to_user,
                              false))))));
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }
}

TEST_F(CollectUserDataActionTest, SelectLoginFailsIfNoOptionAvailable) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* login_details = collect_user_data_proto->mutable_login_details();
  auto* login_option = login_details->add_login_options();
  login_option->mutable_password_manager();
  login_option->set_payload("password_manager");

  ON_CALL(mock_website_login_manager_, OnGetLoginsForUrl(_, _))
      .WillByDefault(
          RunOnceCallback<1>(std::vector<WebsiteLoginManager::Login>{}));

  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              COLLECT_USER_DATA_ERROR))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, SelectContactDetails) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* contact_details_proto =
      collect_user_data_proto->mutable_contact_details();
  contact_details_proto->set_contact_details_name(kMemoryLocation);
  contact_details_proto->set_request_payer_name(true);
  contact_details_proto->set_request_payer_email(true);
  contact_details_proto->set_request_payer_phone(true);

  autofill::AutofillProfile contact_profile;
  contact_profile.SetRawInfo(autofill::ServerFieldType::NAME_FULL,
                             base::UTF8ToUTF16("Marion Mitchell Morrison"));
  contact_profile.SetRawInfo(autofill::ServerFieldType::NAME_FIRST,
                             base::UTF8ToUTF16("Marion"));
  contact_profile.SetRawInfo(autofill::ServerFieldType::NAME_MIDDLE,
                             base::UTF8ToUTF16("Mitchell"));
  contact_profile.SetRawInfo(autofill::ServerFieldType::NAME_LAST,
                             base::UTF8ToUTF16("Morrison"));
  contact_profile.SetRawInfo(autofill::ServerFieldType::EMAIL_ADDRESS,
                             base::UTF8ToUTF16("marion@me.xyz"));
  contact_profile.SetRawInfo(autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER,
                             base::UTF8ToUTF16("16505678910"));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            user_data_.selected_addresses_[kMemoryLocation] =
                std::make_unique<autofill::AutofillProfile>(contact_profile);
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  std::vector<std::string> expected_non_empty_fields = {
      base::NumberToString(
          static_cast<int>(autofill::ServerFieldType::NAME_FULL)),
      base::NumberToString(
          static_cast<int>(autofill::ServerFieldType::NAME_FIRST)),
      base::NumberToString(
          static_cast<int>(autofill::ServerFieldType::NAME_MIDDLE)),
      base::NumberToString(
          static_cast<int>(autofill::ServerFieldType::NAME_LAST)),
      base::NumberToString(
          static_cast<int>(autofill::ServerFieldType::EMAIL_ADDRESS)),
      base::NumberToString(static_cast<int>(
          autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER))};

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::collect_user_data_result,
              AllOf(
                  Property(&CollectUserDataResultProto::payer_email,
                           "marion@me.xyz"),
                  Property(&CollectUserDataResultProto::non_empty_contact_field,
                           IsSupersetOf(expected_non_empty_fields))))))));

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_EQ(user_data_.has_selected_address(kMemoryLocation), true);
  auto* profile = user_data_.selected_address(kMemoryLocation);
  EXPECT_EQ(profile->GetRawInfo(autofill::NAME_FULL),
            base::UTF8ToUTF16("Marion Mitchell Morrison"));
  EXPECT_EQ(profile->GetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER),
            base::UTF8ToUTF16("16505678910"));
  EXPECT_EQ(profile->GetRawInfo(autofill::EMAIL_ADDRESS),
            base::UTF8ToUTF16("marion@me.xyz"));
}

TEST_F(CollectUserDataActionTest,
       ContactDetailsDescriptionFieldsEnumConversion) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* contact_details_proto =
      collect_user_data_proto->mutable_contact_details();
  contact_details_proto->set_contact_details_name(kMemoryLocation);
  contact_details_proto->set_request_payer_name(true);
  contact_details_proto->set_request_payer_email(true);
  contact_details_proto->set_request_payer_phone(true);
  contact_details_proto->add_summary_fields(ContactDetailsProto::EMAIL_ADDRESS);
  contact_details_proto->add_summary_fields(
      ContactDetailsProto::PHONE_HOME_WHOLE_NUMBER);
  contact_details_proto->set_max_number_summary_lines(2);
  contact_details_proto->add_full_fields(ContactDetailsProto::NAME_FULL);
  contact_details_proto->add_full_fields(ContactDetailsProto::EMAIL_ADDRESS);
  contact_details_proto->add_full_fields(
      ContactDetailsProto::PHONE_HOME_WHOLE_NUMBER);
  contact_details_proto->set_max_number_full_lines(3);

  EXPECT_CALL(mock_action_delegate_, CollectUserData(_)).Times(1);
  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            EXPECT_EQ(collect_user_data_options->contact_summary_max_lines, 2);
            EXPECT_EQ(collect_user_data_options->contact_full_max_lines, 3);
            EXPECT_THAT(collect_user_data_options->contact_summary_fields,
                        ElementsAre(EMAIL_ADDRESS, PHONE_HOME_WHOLE_NUMBER));
            EXPECT_THAT(
                collect_user_data_options->contact_full_fields,
                ElementsAre(NAME_FULL, EMAIL_ADDRESS, PHONE_HOME_WHOLE_NUMBER));
          }));

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest,
       ContactDetailsDescriptionDefaultsIfNotSpecified) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* contact_details_proto =
      collect_user_data_proto->mutable_contact_details();
  contact_details_proto->set_contact_details_name(kMemoryLocation);
  contact_details_proto->set_request_payer_name(true);
  contact_details_proto->set_request_payer_email(true);
  contact_details_proto->set_request_payer_phone(true);

  EXPECT_CALL(mock_action_delegate_, CollectUserData(_)).Times(1);
  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            EXPECT_EQ(collect_user_data_options->contact_summary_max_lines, 1);
            EXPECT_EQ(collect_user_data_options->contact_full_max_lines, 2);
            EXPECT_THAT(collect_user_data_options->contact_summary_fields,
                        ElementsAre(EMAIL_ADDRESS, NAME_FULL));
            EXPECT_THAT(collect_user_data_options->contact_full_fields,
                        ElementsAre(NAME_FULL, EMAIL_ADDRESS));
          }));

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, SelectPaymentMethod) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  collect_user_data_proto->set_request_payment_method(true);
  collect_user_data_proto->set_billing_address_name("billing_address");

  autofill::AutofillProfile billing_profile(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(&billing_profile, "Marion", "Mitchell",
                                 "Morrison", "marion@me.xyz", "Fox",
                                 "123 Zoo St.", "unit 5", "Hollywood", "CA",
                                 "91601", "US", "16505678910");

  autofill::CreditCard credit_card(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetCreditCardInfo(&credit_card, "Marion Mitchell",
                                    "4111 1111 1111 1111", "01", "2050",
                                    billing_profile.guid());

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            user_data_.selected_card_ =
                std::make_unique<autofill::CreditCard>(credit_card);
            user_data_.selected_addresses_["billing_address"] =
                std::make_unique<autofill::AutofillProfile>(billing_profile);
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  std::vector<std::string> expected_non_empty_fields = {
      base::NumberToString(
          static_cast<int>(autofill::ServerFieldType::NAME_FIRST)),
      base::NumberToString(
          static_cast<int>(autofill::ServerFieldType::NAME_MIDDLE)),
      base::NumberToString(
          static_cast<int>(autofill::ServerFieldType::NAME_LAST))};

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::collect_user_data_result,
              AllOf(Property(&CollectUserDataResultProto::card_issuer_network,
                             "visa"),
                    Property(&CollectUserDataResultProto::
                                 non_empty_billing_address_field,
                             IsSupersetOf(expected_non_empty_fields))))))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_EQ(user_data_.selected_card_.get() != nullptr, true);
  EXPECT_THAT(user_data_.selected_card_->Compare(credit_card), Eq(0));
}

TEST_F(CollectUserDataActionTest, SelectShippingAddress) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  collect_user_data_proto->set_shipping_address_name(kMemoryLocation);

  autofill::AutofillProfile shipping_address(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(&shipping_address, "Marion", "Mitchell",
                                 "Morrison", "marion@me.xyz", "Fox",
                                 "123 Zoo St.", "unit 5", "Hollywood", "CA",
                                 "91601", "US", "16505678910");

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            user_data_.selected_addresses_[kMemoryLocation] =
                std::make_unique<autofill::AutofillProfile>(shipping_address);
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  std::vector<std::string> expected_non_empty_fields = {
      base::NumberToString(
          static_cast<int>(autofill::ServerFieldType::NAME_FIRST)),
      base::NumberToString(
          static_cast<int>(autofill::ServerFieldType::NAME_MIDDLE)),
      base::NumberToString(
          static_cast<int>(autofill::ServerFieldType::NAME_LAST))};

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::collect_user_data_result,
              Property(
                  &CollectUserDataResultProto::non_empty_shipping_address_field,
                  IsSupersetOf(expected_non_empty_fields)))))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_TRUE(user_data_.has_selected_address(kMemoryLocation));
  EXPECT_EQ(user_data_.selected_addresses_[kMemoryLocation]->Compare(
                shipping_address),
            0);
}

TEST_F(CollectUserDataActionTest, MandatoryPostalCodeWithoutErrorMessageFails) {
  ActionProto action_proto;
  action_proto.mutable_collect_user_data()->set_request_payment_method(true);
  action_proto.mutable_collect_user_data()->set_require_billing_postal_code(
      true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, ContactDetailsCanHandleUtf8) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* contact_details_proto =
      collect_user_data_proto->mutable_contact_details();
  contact_details_proto->set_contact_details_name(kMemoryLocation);
  contact_details_proto->set_request_payer_name(true);
  contact_details_proto->set_request_payer_email(true);

  // Name = 艾丽森 in UTF-8.
  autofill::AutofillProfile contact_profile;
  contact_profile.SetRawInfo(
      autofill::ServerFieldType::NAME_FULL,
      base::UTF8ToUTF16("\xE8\x89\xBE\xE4\xB8\xBD\xE6\xA3\xAE"));
  contact_profile.SetRawInfo(
      autofill::ServerFieldType::EMAIL_ADDRESS,
      base::UTF8ToUTF16("\xE8\x89\xBE\xE4\xB8\xBD\xE6\xA3\xAE@example.com"));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            user_data_.selected_addresses_[kMemoryLocation] =
                std::make_unique<autofill::AutofillProfile>(contact_profile);
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::collect_user_data_result,
              Property(&CollectUserDataResultProto::payer_email,
                       "\xE8\x89\xBE\xE4\xB8\xBD\xE6\xA3\xAE@example.com"))))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_EQ(user_data_.has_selected_address(kMemoryLocation), true);
  auto* profile = user_data_.selected_address(kMemoryLocation);
  EXPECT_EQ(profile->GetRawInfo(autofill::NAME_FULL),
            base::UTF8ToUTF16("\xE8\x89\xBE\xE4\xB8\xBD\xE6\xA3\xAE"));
  EXPECT_EQ(
      profile->GetRawInfo(autofill::EMAIL_ADDRESS),
      base::UTF8ToUTF16("\xE8\x89\xBE\xE4\xB8\xBD\xE6\xA3\xAE@example.com"));
}

TEST_F(CollectUserDataActionTest, UserDataComplete_Contact) {
  UserData user_data;
  CollectUserDataOptions options;
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                        options));

  options.contact_details_name = "profile";
  user_data.selected_addresses_["profile"] =
      std::make_unique<autofill::AutofillProfile>(base::GenerateGUID(),
                                                  kFakeUrl);
  options.request_payer_email = true;
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                         options));

  user_data.selected_addresses_["profile"]->SetRawInfo(
      autofill::ServerFieldType::EMAIL_ADDRESS,
      base::UTF8ToUTF16("joedoe@example.com"));
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                        options));

  options.request_payer_name = true;
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                         options));

  user_data.selected_addresses_["profile"]->SetRawInfo(
      autofill::ServerFieldType::NAME_FULL, base::UTF8ToUTF16("Joe Doe"));
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                        options));

  options.request_payer_phone = true;
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                         options));

  user_data.selected_addresses_["profile"]->SetRawInfo(
      autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER,
      base::UTF8ToUTF16("+1 23 456 789 01"));
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                        options));
}

TEST_F(CollectUserDataActionTest, UserDataComplete_Payment) {
  UserData user_data;
  CollectUserDataOptions options;

  options.request_payment_method = true;
  options.billing_address_name = "billing_address";
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                         options));

  // Valid credit card, but no billing address.
  user_data.selected_card_ =
      std::make_unique<autofill::CreditCard>(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetCreditCardInfo(user_data.selected_card_.get(),
                                    "Marion Mitchell", "4111 1111 1111 1111",
                                    "01", "2050",
                                    /* billing_address_id = */ "");
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                         options));

  // Incomplete billing address.
  user_data.selected_addresses_["billing_address"] =
      std::make_unique<autofill::AutofillProfile>(base::GenerateGUID(),
                                                  kFakeUrl);
  autofill::test::SetProfileInfo(
      user_data.selected_addresses_["billing_address"].get(), "Marion",
      "Mitchell", "Morrison", "marion@me.xyz", "Fox", "123 Zoo St.", "unit 5",
      "Hollywood", "CA",
      /* zipcode = */ "", "US", "16505678910");
  user_data.selected_card_->set_billing_address_id(
      user_data.selected_addresses_["billing_address"]->guid());
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                         options));

  user_data.selected_addresses_["billing_address"]->SetRawInfo(
      autofill::ADDRESS_HOME_ZIP, base::UTF8ToUTF16("91601"));
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                        options));

  // Zip code is optional in Argentinian address.
  user_data.selected_addresses_["billing_address"]->SetRawInfo(
      autofill::ADDRESS_HOME_ZIP, base::UTF8ToUTF16(""));
  user_data.selected_addresses_["billing_address"]->SetRawInfo(
      autofill::ADDRESS_HOME_COUNTRY, base::UTF8ToUTF16("AR"));
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                        options));

  options.require_billing_postal_code = true;
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                         options));

  user_data.selected_addresses_["billing_address"]->SetRawInfo(
      autofill::ADDRESS_HOME_ZIP, base::UTF8ToUTF16("B1675"));
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                        options));

  // Expired credit card.
  user_data.selected_card_->SetRawInfo(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR,
                                       base::UTF8ToUTF16("2019"));
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                         options));
}

TEST_F(CollectUserDataActionTest, UserDataComplete_Terms) {
  UserData user_data;
  CollectUserDataOptions options;

  options.accept_terms_and_conditions_text.assign("Accept T&C");
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                         options));

  user_data.terms_and_conditions_ = REQUIRES_REVIEW;
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                        options));

  user_data.terms_and_conditions_ = ACCEPTED;
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                        options));
}

TEST_F(CollectUserDataActionTest, UserDataComplete_Login) {
  UserData user_data;
  CollectUserDataOptions options;

  options.request_login_choice = true;
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                         options));

  user_data.login_choice_identifier_.assign("1");
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                        options));
}

TEST_F(CollectUserDataActionTest, UserDataComplete_ShippingAddress) {
  UserData user_data;
  CollectUserDataOptions options;
  options.request_shipping = true;
  options.shipping_address_name = "shipping_address";
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                         options));

  // Incomplete address.
  user_data.selected_addresses_["shipping_address"] =
      std::make_unique<autofill::AutofillProfile>(base::GenerateGUID(),
                                                  kFakeUrl);
  autofill::test::SetProfileInfo(
      user_data.selected_addresses_["shipping_address"].get(), "Marion",
      "Mitchell", "Morrison", "marion@me.xyz", "Fox", "123 Zoo St.", "unit 5",
      "Hollywood", "CA",
      /* zipcode = */ "", "US", "16505678910");
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                         options));

  user_data.selected_addresses_["shipping_address"]->SetRawInfo(
      autofill::ADDRESS_HOME_ZIP, base::UTF8ToUTF16("91601"));
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                        options));
}

TEST_F(CollectUserDataActionTest, UserDataComplete_DateTimeRange) {
  UserData user_data;
  CollectUserDataOptions options;
  options.request_date_time_range = true;
  auto* time_slot = options.date_time_range.add_time_slots();
  time_slot->set_label("08:00 AM");
  time_slot->set_comparison_value(0);
  time_slot = options.date_time_range.add_time_slots();
  time_slot->set_label("09:00 AM");
  time_slot->set_comparison_value(1);

  DateProto start_date;
  SetDateProto(&start_date, 2020, 1, 1);
  DateProto end_date;
  SetDateProto(&end_date, 2020, 1, 15);
  user_data.date_time_range_start_date_ = start_date;
  user_data.date_time_range_end_date_ = end_date;
  user_data.date_time_range_start_timeslot_ = 0;
  user_data.date_time_range_end_timeslot_ = 0;

  // Initial selection is valid.
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                        options));

  // Start date not before end date is not ok.
  SetDateProto(&*user_data.date_time_range_start_date_, 2020, 2, 7);
  SetDateProto(&*user_data.date_time_range_end_date_, 2020, 1, 15);
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                         options));

  // Same date with end time > start time is ok.
  SetDateProto(&*user_data.date_time_range_start_date_, 2020, 1, 15);
  SetDateProto(&*user_data.date_time_range_end_date_, 2020, 1, 15);
  user_data.date_time_range_start_timeslot_ = 0;
  user_data.date_time_range_end_timeslot_ = 1;
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                        options));

  // Same date and same time is not ok.
  user_data.date_time_range_start_timeslot_ = 0;
  user_data.date_time_range_end_timeslot_ = 0;
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                         options));

  // Same date and start time > end time is not ok.
  user_data.date_time_range_start_timeslot_ = 1;
  user_data.date_time_range_end_timeslot_ = 0;
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                         options));

  // Start date before end date is ok.
  SetDateProto(&*user_data.date_time_range_start_date_, 2020, 3, 1);
  SetDateProto(&*user_data.date_time_range_end_date_, 2020, 3, 31);
  user_data.date_time_range_start_timeslot_ = 0;
  user_data.date_time_range_end_timeslot_ = 1;
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                        options));
  user_data.date_time_range_start_timeslot_ = 1;
  user_data.date_time_range_end_timeslot_ = 0;
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                        options));

  // Proper date comparison across years.
  SetDateProto(&*user_data.date_time_range_start_date_, 2019, 11, 10);
  SetDateProto(&*user_data.date_time_range_end_date_, 2020, 1, 5);
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                        options));
}

TEST_F(CollectUserDataActionTest,
       UserDataComplete_ChecksGenericUiCompleteness) {
  UserData user_data;
  CollectUserDataOptions options;
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                        options));

  options.additional_model_identifier_to_check = "generic_ui_valid";
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                         options));

  ValueProto invalid;
  invalid.mutable_booleans()->add_values(false);
  user_model_.SetValue("generic_ui_valid", invalid);
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                         options));

  ValueProto valid;
  valid.mutable_booleans()->add_values(true);
  user_model_.SetValue("generic_ui_valid", valid);
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, user_model_,
                                                        options));
}

TEST_F(CollectUserDataActionTest, SelectDateTimeRange) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);

  auto* date_time_range = collect_user_data_proto->mutable_date_time_range();
  SetDateProto(date_time_range->mutable_start_date(), 2020, 1, 1);
  SetDateProto(date_time_range->mutable_end_date(), 2020, 1, 15);
  SetDateProto(date_time_range->mutable_min_date(), 2020, 1, 1);
  SetDateProto(date_time_range->mutable_max_date(), 2020, 12, 31);
  date_time_range->set_start_time_slot(0);
  date_time_range->set_end_time_slot(0);
  date_time_range->set_start_date_label("Start date");
  date_time_range->set_end_date_label("End date");
  date_time_range->set_start_time_label("Start time");
  date_time_range->set_end_time_label("End time");
  date_time_range->set_date_not_set_error("Date not set");
  date_time_range->set_time_not_set_error("Time not set");

  auto* time_slot = date_time_range->add_time_slots();
  time_slot->set_label("08:00 AM");
  time_slot->set_comparison_value(0);
  time_slot = date_time_range->add_time_slots();
  time_slot->set_label("09:00 AM");
  time_slot->set_comparison_value(1);

  DateProto actual_pickup_date;
  DateProto actual_return_date;
  SetDateProto(&actual_pickup_date, 2020, 10, 21);
  SetDateProto(&actual_return_date, 2020, 10, 25);
  int actual_pickup_time = 1;
  int actual_return_time = 1;
  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([&](CollectUserDataOptions* collect_user_data_options) {
            user_data_.date_time_range_start_date_ = actual_pickup_date;
            user_data_.date_time_range_start_timeslot_ = actual_pickup_time;
            user_data_.date_time_range_end_date_ = actual_return_date;
            user_data_.date_time_range_end_timeslot_ = actual_return_time;
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(&ProcessedActionProto::collect_user_data_result,
                   Property(&CollectUserDataResultProto::date_range_start_date,
                            EqualsProto(actual_pickup_date))),
          Property(
              &ProcessedActionProto::collect_user_data_result,
              Property(&CollectUserDataResultProto::date_range_start_timeslot,
                       Eq(actual_pickup_time))),
          Property(&ProcessedActionProto::collect_user_data_result,
                   Property(&CollectUserDataResultProto::date_range_end_date,
                            EqualsProto(actual_return_date))),
          Property(
              &ProcessedActionProto::collect_user_data_result,
              Property(&CollectUserDataResultProto::date_range_end_timeslot,
                       Eq(actual_return_time)))))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, StaticSectionValid) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  auto* static_section =
      collect_user_data_proto->add_additional_prepended_sections();
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
    action.ProcessAction(callback_.Get());
  }

  static_section->set_title("Static section");
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
    action.ProcessAction(callback_.Get());
  }

  static_section->mutable_static_text_section()->set_text("Lorem ipsum.");
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
    action.ProcessAction(callback_.Get());
  }
}

TEST_F(CollectUserDataActionTest, TextInputSectionValid) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  auto* text_input_section =
      collect_user_data_proto->add_additional_prepended_sections();
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
    action.ProcessAction(callback_.Get());
  }

  text_input_section->set_title("Text input section");
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
    action.ProcessAction(callback_.Get());
  }

  auto* input_field =
      text_input_section->mutable_text_input_section()->add_input_fields();
  input_field->set_value("12345");
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
    action.ProcessAction(callback_.Get());
  }

  input_field->set_input_type(TextInputProto::INPUT_ALPHANUMERIC);
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
    action.ProcessAction(callback_.Get());
  }

  input_field->set_client_memory_key("code");
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
    action.ProcessAction(callback_.Get());
  }

  // Duplicate input field fails due to duplicate memory key.
  *text_input_section->mutable_text_input_section()->add_input_fields() =
      *input_field;
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
    action.ProcessAction(callback_.Get());
  }

  text_input_section->mutable_text_input_section()
      ->mutable_input_fields(1)
      ->set_client_memory_key("something else");
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
    action.ProcessAction(callback_.Get());
  }
}

TEST_F(CollectUserDataActionTest, PopupListSectionValid) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  auto* popup_list_section =
      collect_user_data_proto->add_additional_prepended_sections();
  popup_list_section->set_title("Popup list section");
  popup_list_section->mutable_popup_list_section()->set_selection_mandatory(
      false);
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
    action.ProcessAction(callback_.Get());
  }
  popup_list_section->mutable_popup_list_section()->add_item_names("item1");
  popup_list_section->mutable_popup_list_section()->add_item_names("item2");
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
    action.ProcessAction(callback_.Get());
  }

  // Having multiple initial selections fails if multiselect is not allowed
  popup_list_section->mutable_popup_list_section()->add_initial_selection(0);
  popup_list_section->mutable_popup_list_section()->add_initial_selection(1);
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
    action.ProcessAction(callback_.Get());
  }

  popup_list_section->mutable_popup_list_section()->set_allow_multiselect(true);
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
    action.ProcessAction(callback_.Get());
  }

  // If an initial selection is out of bonds of the list of items there is an
  // error.
  popup_list_section->mutable_popup_list_section()->add_initial_selection(2);
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
    action.ProcessAction(callback_.Get());
  }

  popup_list_section->mutable_popup_list_section()->add_item_names("item3");
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
    action.ProcessAction(callback_.Get());
  }
}

TEST_F(CollectUserDataActionTest, TextInputSectionWritesToClientMemory) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            ValueProto value;
            value.mutable_strings()->add_values("modified");
            user_data_.additional_values_["key2"] = value;
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  auto* text_input_section =
      collect_user_data_proto->add_additional_prepended_sections();
  text_input_section->set_title("Text input section");

  auto* input_field_1 =
      text_input_section->mutable_text_input_section()->add_input_fields();
  input_field_1->set_value("initial");
  input_field_1->set_input_type(TextInputProto::INPUT_ALPHANUMERIC);
  input_field_1->set_client_memory_key("key1");

  auto* input_field_2 =
      text_input_section->mutable_text_input_section()->add_input_fields();
  input_field_2->set_value("initial");
  input_field_2->set_input_type(TextInputProto::INPUT_ALPHANUMERIC);
  input_field_2->set_client_memory_key("key2");

  auto* input_field_3 =
      text_input_section->mutable_text_input_section()->add_input_fields();
  input_field_3->set_input_type(TextInputProto::INPUT_ALPHANUMERIC);
  input_field_3->set_client_memory_key("key3");

  EXPECT_FALSE(user_data_.has_additional_value("key1"));
  EXPECT_FALSE(user_data_.has_additional_value("key2"));
  EXPECT_FALSE(user_data_.has_additional_value("key3"));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::collect_user_data_result,
              Property(&CollectUserDataResultProto::set_text_input_memory_keys,
                       UnorderedElementsAre("key1", "key2", "key3")))))));
  action.ProcessAction(callback_.Get());

  ValueProto value1;
  value1.mutable_strings()->add_values("initial");
  ValueProto value2;
  value2.mutable_strings()->add_values("modified");
  ValueProto value3;
  value3.mutable_strings()->add_values("");
  EXPECT_EQ(*user_data_.additional_value("key1"), value1);
  EXPECT_EQ(*user_data_.additional_value("key2"), value2);
  EXPECT_EQ(*user_data_.additional_value("key3"), value3);
}

TEST_F(CollectUserDataActionTest, AllowedBasicCardNetworks) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  collect_user_data_proto->set_billing_address_name("billing_address");

  std::string kSupportedBasicCardNetworks[] = {"amex", "diners",   "discover",
                                               "elo",  "jcb",      "mastercard",
                                               "mir",  "unionpay", "visa"};

  for (const auto& network : kSupportedBasicCardNetworks) {
    *collect_user_data_proto->add_supported_basic_card_networks() = network;
  }

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            user_data_.selected_addresses_["billing_address"] =
                std::make_unique<autofill::AutofillProfile>(
                    base::GenerateGUID(), kFakeUrl);
            autofill::test::SetProfileInfo(
                user_data_.selected_addresses_["billing_address"].get(),
                "Marion", "Mitchell", "Morrison", "marion@me.xyz", "Fox",
                "123 Zoo St.", "unit 5", "Hollywood", "CA", "96043", "US",
                "16505678910");

            user_data_.selected_card_ = std::make_unique<autofill::CreditCard>(
                base::GenerateGUID(), kFakeUrl);
            autofill::test::SetCreditCardInfo(
                user_data_.selected_card_.get(), "Marion Mitchell",
                "4111 1111 1111 1111", "01", "2050",
                user_data_.selected_addresses_["billing_address"]->guid());

            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, InvalidBasicCardNetworks) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);

  *collect_user_data_proto->add_supported_basic_card_networks() = "visa";
  *collect_user_data_proto->add_supported_basic_card_networks() =
      "unknown_network";

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, OverwriteExistingUserData) {
  // Set previous user data state.
  user_data_.terms_and_conditions_ = ACCEPTED;
  ValueProto value1;
  value1.mutable_strings()->add_values("val1");
  ValueProto value2;
  value2.mutable_strings()->add_values("val2");
  ValueProto value3;
  value3.mutable_strings()->add_values("val3");
  user_data_.additional_values_["key1"] = value1;
  user_data_.additional_values_["key2"] = value2;
  user_data_.additional_values_["key3"] = value3;

  // Set options.
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* prepended_section =
      collect_user_data_proto->add_additional_prepended_sections();
  prepended_section->set_title("Text input section");

  auto* input_field_1 =
      prepended_section->mutable_text_input_section()->add_input_fields();
  input_field_1->set_value("initial");
  input_field_1->set_input_type(TextInputProto::INPUT_ALPHANUMERIC);
  input_field_1->set_client_memory_key("key1");

  auto* appended_section =
      collect_user_data_proto->add_additional_appended_sections();
  appended_section->set_title("Text input section 2");
  auto* input_field_2 =
      appended_section->mutable_text_input_section()->add_input_fields();
  input_field_2->set_value("initial");
  input_field_2->set_input_type(TextInputProto::INPUT_ALPHANUMERIC);
  input_field_2->set_client_memory_key("key2");

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([](CollectUserDataOptions* collect_user_data_options) {
            // do not call confirm_callback since we are only looking to test
            // OnShowToUser.
            // Calling confirm_callback then calls OnGetUserData which changes
            // the user_data_.
          }));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_EQ(user_data_.terms_and_conditions_, NOT_SELECTED);
  EXPECT_EQ(user_data_.additional_values_["key1"].strings().values(0),
            "initial");
  EXPECT_EQ(user_data_.additional_values_["key2"].strings().values(0),
            "initial");
  EXPECT_EQ(user_data_.additional_values_["key3"].strings().values(0), "val3");
}

TEST_F(CollectUserDataActionTest, AttachesProfiles) {
  ON_CALL(mock_personal_data_manager_, IsAutofillProfileEnabled)
      .WillByDefault(Return(true));

  autofill::AutofillProfile profile;
  autofill::test::SetProfileInfo(&profile, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "");

  ON_CALL(mock_personal_data_manager_, GetProfiles)
      .WillByDefault(
          Return(std::vector<autofill::AutofillProfile*>({&profile})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            user_data_.selected_addresses_[kMemoryLocation] =
                std::make_unique<autofill::AutofillProfile>(profile);

            EXPECT_THAT(user_data_.available_profiles_, SizeIs(1));
            EXPECT_EQ(user_data_.available_profiles_[0]->Compare(profile), 0);

            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, nullptr);
          }));

  ActionProto action_proto;
  auto* user_data = action_proto.mutable_collect_user_data();
  user_data->set_request_terms_and_conditions(false);
  auto* contact_details = user_data->mutable_contact_details();
  contact_details->set_request_payer_name(true);
  contact_details->set_contact_details_name("contact");

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, InitialSelectsProfileAndShippingAddress) {
  ON_CALL(mock_personal_data_manager_, IsAutofillProfileEnabled)
      .WillByDefault(Return(true));

  autofill::AutofillProfile profile;
  autofill::test::SetProfileInfo(&profile, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "Main St. 18", "",
                                 "abc", "New York", "NY", "10001", "us", "");

  ON_CALL(mock_personal_data_manager_, GetProfiles)
      .WillByDefault(
          Return(std::vector<autofill::AutofillProfile*>({&profile})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            EXPECT_EQ(
                user_data_.selected_addresses_["profile"]->Compare(profile), 0);
            EXPECT_EQ(
                user_data_.selected_addresses_["shipping-address"]->Compare(
                    profile),
                0);

            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, nullptr);
          }));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->set_shipping_address_name("shipping-address");
  auto* contact_details = collect_user_data->mutable_contact_details();
  contact_details->set_request_payer_name(true);
  contact_details->set_contact_details_name("profile");

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, ShippingAddressSectionCustomTitle) {
  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            // do not call confirm_callback since we are only looking to test
            // |collect_user_data_options|.
            EXPECT_EQ(collect_user_data_options->shipping_address_section_title,
                      "custom title");
          }));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_shipping_address_section_title("custom title");

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, ShippingAddressSectionDefaultTitle) {
  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            // do not call confirm_callback since we are only looking to test
            // |collect_user_data_options|.
            EXPECT_EQ(
                collect_user_data_options->shipping_address_section_title,
                l10n_util::GetStringUTF8(IDS_PAYMENTS_SHIPPING_ADDRESS_LABEL));
          }));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_shipping_address_section_title("");

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, ContactDetailsSectionCustomTitle) {
  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            // do not call confirm_callback since we are only looking to test
            // |collect_user_data_options|.
            EXPECT_EQ(collect_user_data_options->contact_details_section_title,
                      "custom title");
          }));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->mutable_contact_details()
      ->set_contact_details_section_title("custom title");

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, ContactDetailsSectionDefaultTitle) {
  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            // do not call confirm_callback since we are only looking to test
            // |collect_user_data_options|.
            EXPECT_EQ(
                collect_user_data_options->shipping_address_section_title,
                l10n_util::GetStringUTF8(IDS_PAYMENTS_CONTACT_DETAILS_LABEL));
          }));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->mutable_contact_details()
      ->set_contact_details_section_title("");

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, InitialSelectsProfileFromDefaultEmail) {
  ON_CALL(mock_personal_data_manager_, IsAutofillProfileEnabled)
      .WillByDefault(Return(true));

  autofill::AutofillProfile profile_a;
  autofill::test::SetProfileInfo(&profile_a, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "");

  autofill::AutofillProfile profile_b;
  autofill::test::SetProfileInfo(&profile_b, "Berta", "", "West",
                                 "berta.west@gmail.com", "", "", "", "", "", "",
                                 "", "");

  ON_CALL(mock_personal_data_manager_, GetProfiles)
      .WillByDefault(Return(
          std::vector<autofill::AutofillProfile*>({&profile_a, &profile_b})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            EXPECT_EQ(
                user_data_.selected_addresses_["profile"]->Compare(profile_b),
                0);

            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, nullptr);
          }));

  ActionProto action_proto;
  auto* user_data = action_proto.mutable_collect_user_data();
  user_data->set_request_terms_and_conditions(false);
  auto* contact_details = user_data->mutable_contact_details();
  contact_details->set_request_payer_name(true);
  contact_details->set_request_payer_email(true);
  contact_details->set_contact_details_name("profile");

  ON_CALL(mock_action_delegate_, GetEmailAddressForAccessTokenAccount())
      .WillByDefault(Return("berta.west@gmail.com"));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, KeepsSelectedProfileAndShippingAddress) {
  ON_CALL(mock_personal_data_manager_, IsAutofillProfileEnabled)
      .WillByDefault(Return(true));

  autofill::AutofillProfile profile;
  autofill::test::SetProfileInfo(&profile, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "Main St. 18", "",
                                 "abc", "New York", "NY", "10001", "us", "");

  ON_CALL(mock_personal_data_manager_, GetProfiles)
      .WillByDefault(
          Return(std::vector<autofill::AutofillProfile*>({&profile})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            EXPECT_EQ(
                user_data_.selected_addresses_["profile"]->Compare(profile), 0);
            EXPECT_EQ(
                user_data_.selected_addresses_["shipping_address"]->Compare(
                    profile),
                0);

            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, nullptr);
          }));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->set_shipping_address_name("shipping_address");
  auto* contact_details = collect_user_data->mutable_contact_details();
  contact_details->set_request_payer_name(true);
  contact_details->set_contact_details_name("profile");

  // Set previous user data.
  user_data_.selected_addresses_["profile"] =
      std::make_unique<autofill::AutofillProfile>(profile);
  user_data_.selected_addresses_["shipping_address"] =
      std::make_unique<autofill::AutofillProfile>(profile);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, ResetsContactAndShippingIfNoLongerInList) {
  ON_CALL(mock_personal_data_manager_, IsAutofillProfileEnabled)
      .WillByDefault(Return(true));

  autofill::AutofillProfile profile;
  autofill::test::SetProfileInfo(&profile, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "");

  ON_CALL(mock_personal_data_manager_, GetProfiles)
      .WillByDefault(
          Return(std::vector<autofill::AutofillProfile*>({&profile})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            EXPECT_EQ(user_data_.selected_addresses_["profile"], nullptr);
            EXPECT_EQ(
                user_data_.selected_addresses_["shipping_address"]->Compare(
                    profile),
                0);

            // Do not call the callback. We're only interested in the state.
          }));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->mutable_contact_details();
  collect_user_data->set_shipping_address_name("shipping_address");
  auto* contact_details = collect_user_data->mutable_contact_details();
  contact_details->set_contact_details_name("profile");

  // Set previous user data.
  autofill::AutofillProfile selected_profile;
  autofill::test::SetProfileInfo(&selected_profile, "Berta", "", "West",
                                 "berta.west@gmail.com", "", "", "", "", "", "",
                                 "", "");

  user_data_.selected_addresses_["profile"] =
      std::make_unique<autofill::AutofillProfile>(selected_profile);
  user_data_.selected_addresses_["shipping_address"] =
      std::make_unique<autofill::AutofillProfile>(selected_profile);

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, AttachesCreditCardsWithAddress) {
  ON_CALL(mock_personal_data_manager_, IsAutofillCreditCardEnabled)
      .WillByDefault(Return(true));
  ON_CALL(mock_personal_data_manager_, ShouldSuggestServerCards)
      .WillByDefault(Return(true));

  autofill::AutofillProfile billing_address;
  autofill::test::SetProfileInfo(&billing_address, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "Baker Street 221b",
                                 "", "London", "", "WC2N 5DU", "UK", "+44");

  ON_CALL(mock_personal_data_manager_, GetProfileByGUID("GUID"))
      .WillByDefault(Return(&billing_address));

  autofill::CreditCard card_with_address;
  autofill::test::SetCreditCardInfo(&card_with_address, "Adam West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "GUID");

  ON_CALL(mock_personal_data_manager_, GetCreditCards())
      .WillByDefault(
          Return(std::vector<autofill::CreditCard*>({&card_with_address})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            EXPECT_THAT(user_data_.available_payment_instruments_, SizeIs(1));
            EXPECT_EQ(
                user_data_.available_payment_instruments_[0]->card->Compare(
                    card_with_address),
                0);
            EXPECT_EQ(user_data_.available_payment_instruments_[0]
                          ->billing_address->Compare(billing_address),
                      0);

            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  ActionProto action_proto;
  auto* user_data = action_proto.mutable_collect_user_data();
  user_data->set_request_terms_and_conditions(false);
  user_data->add_supported_basic_card_networks("visa");

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, AttachesCreditCardsWithoutAddress) {
  ON_CALL(mock_personal_data_manager_, IsAutofillCreditCardEnabled)
      .WillByDefault(Return(true));
  ON_CALL(mock_personal_data_manager_, ShouldSuggestServerCards)
      .WillByDefault(Return(true));

  autofill::CreditCard card_without_address;
  autofill::test::SetCreditCardInfo(&card_without_address, "Adam West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "");

  ON_CALL(mock_personal_data_manager_, GetCreditCards())
      .WillByDefault(
          Return(std::vector<autofill::CreditCard*>({&card_without_address})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            EXPECT_THAT(user_data_.available_payment_instruments_, SizeIs(1));
            EXPECT_EQ(
                user_data_.available_payment_instruments_[0]->card->Compare(
                    card_without_address),
                0);
            EXPECT_EQ(user_data_.available_payment_instruments_[0]
                          ->billing_address.get(),
                      nullptr);

            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  ActionProto action_proto;
  auto* user_data = action_proto.mutable_collect_user_data();
  user_data->set_request_terms_and_conditions(false);
  user_data->add_supported_basic_card_networks("visa");

  EXPECT_CALL(mock_personal_data_manager_, GetProfileByGUID(_)).Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, AttachesCreditCardsForEmptyNetworksList) {
  ON_CALL(mock_personal_data_manager_, IsAutofillCreditCardEnabled)
      .WillByDefault(Return(true));
  ON_CALL(mock_personal_data_manager_, ShouldSuggestServerCards)
      .WillByDefault(Return(true));

  autofill::CreditCard card;
  autofill::test::SetCreditCardInfo(&card, "Adam West", "4111111111111111", "1",
                                    "2050",
                                    /* billing_address_id= */ "");

  ON_CALL(mock_personal_data_manager_, GetCreditCards())
      .WillByDefault(Return(std::vector<autofill::CreditCard*>({&card})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            EXPECT_THAT(user_data_.available_payment_instruments_, SizeIs(1));
            EXPECT_EQ(
                user_data_.available_payment_instruments_[0]->card->Compare(
                    card),
                0);

            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  ActionProto action_proto;
  auto* user_data = action_proto.mutable_collect_user_data();
  user_data->set_request_terms_and_conditions(false);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, InitialSelectsCardAndAddress) {
  ON_CALL(mock_personal_data_manager_, IsAutofillCreditCardEnabled)
      .WillByDefault(Return(true));
  ON_CALL(mock_personal_data_manager_, ShouldSuggestServerCards)
      .WillByDefault(Return(true));

  autofill::AutofillProfile billing_address;
  autofill::test::SetProfileInfo(&billing_address, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "Baker Street 221b",
                                 "", "London", "", "WC2N 5DU", "UK", "+44");

  ON_CALL(mock_personal_data_manager_, GetProfileByGUID("GUID"))
      .WillByDefault(Return(&billing_address));

  autofill::CreditCard card_with_address;
  autofill::test::SetCreditCardInfo(&card_with_address, "Adam West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "GUID");

  ON_CALL(mock_personal_data_manager_, GetCreditCards())
      .WillByDefault(
          Return(std::vector<autofill::CreditCard*>({&card_with_address})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            EXPECT_EQ(user_data_.selected_card_->Compare(card_with_address), 0);
            EXPECT_EQ(
                user_data_.selected_addresses_["billing_address"]->Compare(
                    billing_address),
                0);

            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, nullptr);
          }));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->add_supported_basic_card_networks("visa");
  collect_user_data->set_request_payment_method(true);
  collect_user_data->set_billing_address_name("billing_address");

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, KeepsSelectedCardAndAddress) {
  ON_CALL(mock_personal_data_manager_, IsAutofillCreditCardEnabled)
      .WillByDefault(Return(true));
  ON_CALL(mock_personal_data_manager_, ShouldSuggestServerCards)
      .WillByDefault(Return(true));

  autofill::AutofillProfile billing_address;
  autofill::test::SetProfileInfo(&billing_address, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "Baker Street 221b",
                                 "", "London", "", "WC2N 5DU", "UK", "+44");

  ON_CALL(mock_personal_data_manager_, GetProfileByGUID("GUID"))
      .WillByDefault(Return(&billing_address));

  autofill::CreditCard card_with_address;
  autofill::test::SetCreditCardInfo(&card_with_address, "Adam West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "GUID");

  ON_CALL(mock_personal_data_manager_, GetCreditCards())
      .WillByDefault(
          Return(std::vector<autofill::CreditCard*>({&card_with_address})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            EXPECT_EQ(user_data_.selected_card_->Compare(card_with_address), 0);
            EXPECT_EQ(
                user_data_.selected_addresses_["billing_address"]->Compare(
                    billing_address),
                0);

            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, nullptr);
          }));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->add_supported_basic_card_networks("visa");
  collect_user_data->set_billing_address_name("billing_address");

  // Set previous user data.
  user_data_.selected_card_ =
      std::make_unique<autofill::CreditCard>(card_with_address);
  user_data_.selected_addresses_["billing_address"] =
      std::make_unique<autofill::AutofillProfile>(billing_address);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, ResetsCardAndAddressIfNoLongerInList) {
  ON_CALL(mock_personal_data_manager_, IsAutofillCreditCardEnabled)
      .WillByDefault(Return(true));
  ON_CALL(mock_personal_data_manager_, ShouldSuggestServerCards)
      .WillByDefault(Return(true));

  autofill::AutofillProfile billing_address;
  autofill::test::SetProfileInfo(&billing_address, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "Baker Street 221b",
                                 "", "London", "", "WC2N 5DU", "UK", "+44");

  ON_CALL(mock_personal_data_manager_, GetProfileByGUID("GUID"))
      .WillByDefault(Return(&billing_address));

  autofill::CreditCard card_with_address;
  autofill::test::SetCreditCardInfo(&card_with_address, "Adam West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "GUID");

  ON_CALL(mock_personal_data_manager_, GetCreditCards())
      .WillByDefault(
          Return(std::vector<autofill::CreditCard*>({&card_with_address})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            EXPECT_EQ(user_data_.selected_card_, nullptr);
            EXPECT_EQ(user_data_.selected_addresses_["billing_address"],
                      nullptr);

            // Do not call the callback. We're only interested in the state.
          }));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->add_supported_basic_card_networks("visa");
  collect_user_data->set_billing_address_name("billing_address");

  // Set previous user data.
  autofill::CreditCard selected_card;
  autofill::test::SetCreditCardInfo(&selected_card, "Berta West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "");
  autofill::AutofillProfile selected_address;
  autofill::test::SetProfileInfo(
      &selected_address, "Berta", "", "West", "berta.west@gmail.com", "",
      "Baker Street 221b", "", "London", "", "WC2N 5DU", "UK", "+44");

  user_data_.selected_card_ =
      std::make_unique<autofill::CreditCard>(selected_card);
  user_data_.selected_addresses_["billing_address"] =
      std::make_unique<autofill::AutofillProfile>(selected_address);

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, GenericUiModelWritesToProtoResult) {
  ModelProto::ModelValue value_1;
  value_1.set_identifier("value_1_key");
  value_1.mutable_value()->mutable_strings()->add_values(
      "value_1_initial_value");
  ModelProto::ModelValue value_1_modified;
  value_1_modified.set_identifier("value_1_key");
  ModelProto::ModelValue value_2;
  value_2.set_identifier("value_2_key");
  value_2.mutable_value()->mutable_strings()->add_values(
      "value_2_initial_value");
  ModelProto::ModelValue value_2_modified;
  value_2_modified.set_identifier("value_2_key");
  value_2_modified.mutable_value()->mutable_strings()->add_values(
      "value_2_modified");
  ModelProto::ModelValue value_3;
  value_3.set_identifier("value_3_key");
  value_3.mutable_value()->mutable_strings()->add_values(
      "value_3_initial_value");
  ModelProto::ModelValue value_3_modified;
  value_3_modified.set_identifier("value_3_key");
  value_3_modified.mutable_value()->mutable_strings()->add_values(
      "value_3_modified");
  ModelProto::ModelValue value_4;
  value_4.set_identifier("value_4_key");
  value_4.mutable_value()->mutable_strings()->add_values(
      "value_4_initial_value");

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            user_model_.SetValue("value_1_key", value_1_modified.value());
            user_model_.SetValue("value_2_key", value_2_modified.value());
            user_model_.SetValue("value_3_key", value_3_modified.value());
            // Leave value_4 at initial value.

            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  auto* proto_model_prepended =
      collect_user_data->mutable_generic_user_interface_prepended()
          ->mutable_model();
  *proto_model_prepended->add_values() = value_1;
  *proto_model_prepended->add_values() = value_2;
  auto* proto_model_appended =
      collect_user_data->mutable_generic_user_interface_appended()
          ->mutable_model();
  *proto_model_appended->add_values() = value_3;
  *proto_model_appended->add_values() = value_4;

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(&ProcessedActionProto::collect_user_data_result,
                   Property(&CollectUserDataResultProto::model,
                            Property(&ModelProto::values,
                                     UnorderedElementsAre(
                                         value_1_modified, value_2_modified,
                                         value_3_modified, value_4))))))));

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, ClearUserDataIfRequested) {
  ON_CALL(mock_personal_data_manager_, IsAutofillCreditCardEnabled)
      .WillByDefault(Return(true));
  ON_CALL(mock_personal_data_manager_, IsAutofillProfileEnabled)
      .WillByDefault(Return(true));
  ON_CALL(mock_personal_data_manager_, ShouldSuggestServerCards)
      .WillByDefault(Return(true));

  autofill::AutofillProfile address_a;
  autofill::test::SetProfileInfo(&address_a, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "Baker Street 221b",
                                 "", "London", "", "WC2N 5DU", "UK", "+44");
  autofill::AutofillProfile address_b;
  autofill::test::SetProfileInfo(
      &address_b, "Berta", "", "West", "berta.west@gmail.com", "",
      "Baker Street 221b", "", "London", "", "WC2N 5DU", "UK", "+44");

  ON_CALL(mock_personal_data_manager_, GetProfileByGUID("card_a"))
      .WillByDefault(Return(&address_a));
  ON_CALL(mock_personal_data_manager_, GetProfileByGUID("card_b"))
      .WillByDefault(Return(&address_b));

  autofill::CreditCard card_a;
  autofill::test::SetCreditCardInfo(&card_a, "Adam West", "4111111111111111",
                                    "1", "2050",
                                    /* billing_address_id= */ "card_a");

  autofill::CreditCard card_b;
  autofill::test::SetCreditCardInfo(&card_b, "Berta West", "4111111111111111",
                                    "1", "2050",
                                    /* billing_address_id= */ "card_b");

  ON_CALL(mock_personal_data_manager_, GetCreditCards())
      .WillByDefault(
          Return(std::vector<autofill::CreditCard*>({&card_a, &card_b})));

  ON_CALL(mock_personal_data_manager_, GetProfiles)
      .WillByDefault(Return(
          std::vector<autofill::AutofillProfile*>({&address_a, &address_b})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            EXPECT_EQ(user_data_.selected_card_->Compare(card_a), 0);
            EXPECT_EQ(
                user_data_.selected_addresses_["billing"]->Compare(address_a),
                0);
            EXPECT_EQ(
                user_data_.selected_addresses_["contact"]->Compare(address_a),
                0);
            EXPECT_EQ(
                user_data_.selected_addresses_["shipping"]->Compare(address_a),
                0);
            EXPECT_EQ(user_data_.selected_login_, base::nullopt);

            // Do not call the callback. We're only interested in the state.
          }));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_payment_method(true);
  collect_user_data->set_billing_address_name("billing");
  collect_user_data->set_shipping_address_name("shipping");
  collect_user_data->mutable_contact_details()->set_request_payer_name(true);
  collect_user_data->mutable_contact_details()->set_contact_details_name(
      "contact");
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->add_supported_basic_card_networks("visa");
  collect_user_data->set_clear_previous_credit_card_selection(true);
  collect_user_data->set_clear_previous_login_selection(true);
  collect_user_data->add_clear_previous_profile_selection("billing");
  collect_user_data->add_clear_previous_profile_selection("contact");
  collect_user_data->add_clear_previous_profile_selection("shipping");

  // Set previous user data to the second card/profile. If clear works
  // correctly, the action should default to the first card/profile.
  user_data_.selected_card_ = std::make_unique<autofill::CreditCard>(card_b);
  user_data_.selected_addresses_["billing"] =
      std::make_unique<autofill::AutofillProfile>(address_b);
  user_data_.selected_addresses_["contact"] =
      std::make_unique<autofill::AutofillProfile>(address_b);
  user_data_.selected_addresses_["shipping"] =
      std::make_unique<autofill::AutofillProfile>(address_b);
  user_data_.selected_login_ =
      WebsiteLoginManager::Login(GURL("http://www.example.com"), "username");

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, LinkClickWritesPartialUserData) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(true);
  collect_user_data_proto->set_accept_terms_and_conditions_text(
      "terms and conditions");
  collect_user_data_proto->set_show_terms_as_checkbox(true);
  auto* contact_details_proto =
      collect_user_data_proto->mutable_contact_details();
  contact_details_proto->set_contact_details_name(kMemoryLocation);
  contact_details_proto->set_request_payer_name(true);
  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            ValueProto value;
            value.mutable_strings()->add_values("modified");
            user_data_.additional_values_["key1"] = value;
            std::move(collect_user_data_options->terms_link_callback)
                .Run(1, &user_data_, &user_model_);
          }));

  auto* text_input_section =
      collect_user_data_proto->add_additional_prepended_sections();
  text_input_section->set_title("Text input section");

  auto* input_field_1 =
      text_input_section->mutable_text_input_section()->add_input_fields();
  input_field_1->set_value("initial");
  input_field_1->set_input_type(TextInputProto::INPUT_ALPHANUMERIC);
  input_field_1->set_client_memory_key("key1");

  CollectUserDataAction action(&mock_action_delegate_, action_proto);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::collect_user_data_result,
              Property(&CollectUserDataResultProto::set_text_input_memory_keys,
                       UnorderedElementsAre("key1")))))));
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, ConfirmButtonChip) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* confirm_chip = collect_user_data_proto->mutable_confirm_chip();
  confirm_chip->set_text("Custom text");
  confirm_chip->set_icon(ICON_CLEAR);
  confirm_chip->set_sticky(true);

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            EXPECT_EQ(collect_user_data_options->confirm_action.chip().text(),
                      "Custom text");
            EXPECT_EQ(collect_user_data_options->confirm_action.chip().icon(),
                      ICON_CLEAR);
            EXPECT_EQ(collect_user_data_options->confirm_action.chip().sticky(),
                      true);
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, ConfirmButtonFallbackText) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            EXPECT_EQ(collect_user_data_options->confirm_action.chip().text(),
                      l10n_util::GetStringUTF8(
                          IDS_AUTOFILL_ASSISTANT_PAYMENT_INFO_CONFIRM));
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

}  // namespace
}  // namespace autofill_assistant
