// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/collect_user_data_action.h"

#include <utility>

#include "base/guid.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/mock_personal_data_manager.h"
#include "components/autofill_assistant/browser/mock_website_login_fetcher.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {
const char kFakeUrl[] = "https://www.example.com";
const char kFakeUsername[] = "user@example.com";
const char kFakePassword[] = "example_password";

const char kMemoryLocation[] = "billing";
}  // namespace

namespace autofill_assistant {
namespace {

void SetDateTimeProto(DateTimeProto* proto,
                      int year,
                      int month,
                      int day,
                      int hour,
                      int minute,
                      int second) {
  proto->mutable_date()->set_year(year);
  proto->mutable_date()->set_month(month);
  proto->mutable_date()->set_day(day);
  proto->mutable_time()->set_hour(hour);
  proto->mutable_time()->set_minute(minute);
  proto->mutable_time()->set_second(second);
}

MATCHER_P(EqualsProto, message, "") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Property;
using ::testing::Return;
using ::testing::SizeIs;

void SetRequiredTermsFields(CollectUserDataProto* data,
                            bool request_terms_and_conditions = false) {
  data->set_thirdparty_privacy_notice_text("privacy");

  if (request_terms_and_conditions) {
    data->set_accept_terms_and_conditions_text("terms and conditions");
    data->set_terms_require_review_text("terms review");
  }
  data->set_request_terms_and_conditions(request_terms_and_conditions);
}

class CollectUserDataActionTest : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    ON_CALL(mock_action_delegate_, GetClientMemory)
        .WillByDefault(Return(&client_memory_));
    ON_CALL(mock_action_delegate_, GetPersonalDataManager)
        .WillByDefault(Return(&mock_personal_data_manager_));
    ON_CALL(mock_action_delegate_, GetWebsiteLoginFetcher)
        .WillByDefault(Return(&mock_website_login_fetcher_));
    ON_CALL(mock_action_delegate_, CollectUserData(_, _))
        .WillByDefault(Invoke([](std::unique_ptr<CollectUserDataOptions>
                                     collect_user_data_options,
                                 std::unique_ptr<UserData> user_data) {
          std::move(collect_user_data_options->confirm_callback)
              .Run(std::move(user_data));
        }));

    ON_CALL(mock_website_login_fetcher_, OnGetLoginsForUrl(_, _))
        .WillByDefault(
            RunOnceCallback<1>(std::vector<WebsiteLoginFetcher::Login>{
                WebsiteLoginFetcher::Login(GURL(kFakeUrl), kFakeUsername)}));
    ON_CALL(mock_website_login_fetcher_, OnGetPasswordForLogin(_, _))
        .WillByDefault(RunOnceCallback<1>(true, kFakePassword));

    content::WebContentsTester::For(web_contents())
        ->SetLastCommittedURL(GURL(kFakeUrl));
    ON_CALL(mock_action_delegate_, GetWebContents())
        .WillByDefault(Return(web_contents()));
  }

 protected:
  base::MockCallback<Action::ProcessActionCallback> callback_;
  MockPersonalDataManager mock_personal_data_manager_;
  MockWebsiteLoginFetcher mock_website_login_fetcher_;
  MockActionDelegate mock_action_delegate_;
  ClientMemory client_memory_;
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
  collect_user_data_proto->set_thirdparty_privacy_notice_text("privacy");
  collect_user_data_proto->set_request_terms_and_conditions(false);

  ON_CALL(mock_action_delegate_, CollectUserData(_, _))
      .WillByDefault(Invoke(
          [](std::unique_ptr<CollectUserDataOptions> collect_user_data_options,
             std::unique_ptr<UserData> user_data) {
            user_data->succeed = true;
            user_data->terms_and_conditions = ACCEPTED;
            std::move(collect_user_data_options->confirm_callback)
                .Run(std::move(user_data));
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
  collect_user_data_proto->set_thirdparty_privacy_notice_text("privacy");
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
  collect_user_data_proto->set_thirdparty_privacy_notice_text("privacy");
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
  collect_user_data_proto->set_thirdparty_privacy_notice_text("privacy");
  collect_user_data_proto->set_request_terms_and_conditions(true);
  collect_user_data_proto->set_accept_terms_and_conditions_text(
      "terms and conditions");
  collect_user_data_proto->set_show_terms_as_checkbox(true);

  ON_CALL(mock_action_delegate_, CollectUserData(_, _))
      .WillByDefault(Invoke(
          [](std::unique_ptr<CollectUserDataOptions> collect_user_data_options,
             std::unique_ptr<UserData> user_data) {
            user_data->succeed = true;
            user_data->terms_and_conditions = ACCEPTED;
            std::move(collect_user_data_options->confirm_callback)
                .Run(std::move(user_data));
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
  collect_user_data_proto->set_thirdparty_privacy_notice_text("privacy");
  collect_user_data_proto->set_request_terms_and_conditions(true);
  collect_user_data_proto->set_accept_terms_and_conditions_text(
      "terms and conditions");
  collect_user_data_proto->set_show_terms_as_checkbox(false);
  collect_user_data_proto->set_terms_require_review_text("terms review");

  ON_CALL(mock_action_delegate_, CollectUserData(_, _))
      .WillByDefault(Invoke(
          [](std::unique_ptr<CollectUserDataOptions> collect_user_data_options,
             std::unique_ptr<UserData> user_data) {
            user_data->succeed = true;
            user_data->terms_and_conditions = ACCEPTED;
            std::move(collect_user_data_options->confirm_callback)
                .Run(std::move(user_data));
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

TEST_F(CollectUserDataActionTest, PromptIsShown) {
  const char kPrompt[] = "Some message.";

  ActionProto action_proto;
  SetRequiredTermsFields(action_proto.mutable_collect_user_data());
  action_proto.mutable_collect_user_data()->set_prompt(kPrompt);

  EXPECT_CALL(mock_action_delegate_, SetStatusMessage(kPrompt));
  EXPECT_CALL(callback_, Run(_));

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, SelectLogin) {
  ActionProto action_proto;
  SetRequiredTermsFields(action_proto.mutable_collect_user_data());
  auto* login_details =
      action_proto.mutable_collect_user_data()->mutable_login_details();
  auto* login_option = login_details->add_login_options();
  login_option->mutable_password_manager();
  login_option->set_payload("payload");

  // Action should fetch the logins, but not the passwords.
  EXPECT_CALL(mock_website_login_fetcher_, OnGetLoginsForUrl(GURL(kFakeUrl), _))
      .Times(1);
  EXPECT_CALL(mock_website_login_fetcher_, OnGetPasswordForLogin(_, _))
      .Times(0);

  ON_CALL(mock_action_delegate_, CollectUserData(_, _))
      .WillByDefault(Invoke(
          [](std::unique_ptr<CollectUserDataOptions> collect_user_data_options,
             std::unique_ptr<UserData> user_data) {
            user_data->succeed = true;
            user_data->login_choice_identifier.assign(
                collect_user_data_options->login_choices[0].identifier);
            std::move(collect_user_data_options->confirm_callback)
                .Run(std::move(user_data));
          }));

  EXPECT_CALL(callback_,
              Run(Pointee(AllOf(
                  Property(&ProcessedActionProto::status, ACTION_APPLIED),
                  Property(&ProcessedActionProto::collect_user_data_result,
                           Property(&CollectUserDataResultProto::login_payload,
                                    "payload"))))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, LoginChoiceAutomaticIfNoOtherOptions) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  SetRequiredTermsFields(collect_user_data_proto);
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* login_details = collect_user_data_proto->mutable_login_details();
  auto* login_option = login_details->add_login_options();
  login_option->mutable_custom()->set_label("Guest Checkout");
  login_option->set_payload("guest");
  login_option->set_choose_automatically_if_no_other_options(true);
  login_option = login_details->add_login_options();
  login_option->mutable_password_manager();
  login_option->set_payload("password_manager");

  ON_CALL(mock_website_login_fetcher_, OnGetLoginsForUrl(_, _))
      .WillByDefault(
          RunOnceCallback<1>(std::vector<WebsiteLoginFetcher::Login>{}));

  EXPECT_CALL(mock_action_delegate_, CollectUserData(_, _)).Times(0);
  EXPECT_CALL(callback_,
              Run(Pointee(AllOf(
                  Property(&ProcessedActionProto::status, ACTION_APPLIED),
                  Property(&ProcessedActionProto::collect_user_data_result,
                           Property(&CollectUserDataResultProto::login_payload,
                                    "guest"))))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, SelectLoginFailsIfNoOptionAvailable) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  SetRequiredTermsFields(collect_user_data_proto);
  auto* login_details = collect_user_data_proto->mutable_login_details();
  auto* login_option = login_details->add_login_options();
  login_option->mutable_password_manager();
  login_option->set_payload("password_manager");

  ON_CALL(mock_website_login_fetcher_, OnGetLoginsForUrl(_, _))
      .WillByDefault(
          RunOnceCallback<1>(std::vector<WebsiteLoginFetcher::Login>{}));

  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              COLLECT_USER_DATA_ERROR))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, SelectContactDetails) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  SetRequiredTermsFields(collect_user_data_proto);
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

  ON_CALL(mock_action_delegate_, CollectUserData(_, _))
      .WillByDefault(Invoke(
          [=](std::unique_ptr<CollectUserDataOptions> collect_user_data_options,
              std::unique_ptr<UserData> user_data) {
            user_data->succeed = true;
            user_data->contact_profile =
                std::make_unique<autofill::AutofillProfile>(contact_profile);
            std::move(collect_user_data_options->confirm_callback)
                .Run(std::move(user_data));
          }));

  EXPECT_CALL(callback_,
              Run(Pointee(AllOf(
                  Property(&ProcessedActionProto::status, ACTION_APPLIED),
                  Property(&ProcessedActionProto::collect_user_data_result,
                           Property(&CollectUserDataResultProto::payer_email,
                                    "marion@me.xyz"))))));

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_EQ(client_memory_.has_selected_address(kMemoryLocation), true);
  auto* profile = client_memory_.selected_address(kMemoryLocation);
  EXPECT_EQ(profile->GetRawInfo(autofill::NAME_FULL),
            base::UTF8ToUTF16("Marion Mitchell Morrison"));
  EXPECT_EQ(profile->GetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER),
            base::UTF8ToUTF16("16505678910"));
  EXPECT_EQ(profile->GetRawInfo(autofill::EMAIL_ADDRESS),
            base::UTF8ToUTF16("marion@me.xyz"));
}

TEST_F(CollectUserDataActionTest, SelectPaymentMethod) {
  ActionProto action_proto;
  SetRequiredTermsFields(action_proto.mutable_collect_user_data());
  action_proto.mutable_collect_user_data()->set_request_payment_method(true);
  action_proto.mutable_collect_user_data()->set_request_terms_and_conditions(
      false);

  autofill::AutofillProfile billing_profile(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(&billing_profile, "Marion", "Mitchell",
                                 "Morrison", "marion@me.xyz", "Fox",
                                 "123 Zoo St.", "unit 5", "Hollywood", "CA",
                                 "91601", "US", "16505678910");

  autofill::CreditCard credit_card(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetCreditCardInfo(&credit_card, "Marion Mitchell",
                                    "4111 1111 1111 1111", "01", "2020",
                                    billing_profile.guid());

  ON_CALL(mock_action_delegate_, CollectUserData(_, _))
      .WillByDefault(Invoke(
          [=](std::unique_ptr<CollectUserDataOptions> collect_user_data_options,
              std::unique_ptr<UserData> user_data) {
            user_data->card =
                std::make_unique<autofill::CreditCard>(credit_card);
            user_data->billing_address =
                std::make_unique<autofill::AutofillProfile>(billing_profile);
            user_data->succeed = true;
            std::move(collect_user_data_options->confirm_callback)
                .Run(std::move(user_data));
          }));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(&ProcessedActionProto::collect_user_data_result,
                   Property(&CollectUserDataResultProto::card_issuer_network,
                            "visa"))))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_EQ(client_memory_.has_selected_card(), true);
  EXPECT_THAT(client_memory_.selected_card()->Compare(credit_card), Eq(0));
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
  SetRequiredTermsFields(collect_user_data_proto);
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

  ON_CALL(mock_action_delegate_, CollectUserData(_, _))
      .WillByDefault(Invoke(
          [=](std::unique_ptr<CollectUserDataOptions> collect_user_data_options,
              std::unique_ptr<UserData> user_data) {
            user_data->succeed = true;
            user_data->contact_profile =
                std::make_unique<autofill::AutofillProfile>(contact_profile);
            std::move(collect_user_data_options->confirm_callback)
                .Run(std::move(user_data));
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

  EXPECT_EQ(client_memory_.has_selected_address(kMemoryLocation), true);
  auto* profile = client_memory_.selected_address(kMemoryLocation);
  EXPECT_EQ(profile->GetRawInfo(autofill::NAME_FULL),
            base::UTF8ToUTF16("\xE8\x89\xBE\xE4\xB8\xBD\xE6\xA3\xAE"));
  EXPECT_EQ(
      profile->GetRawInfo(autofill::EMAIL_ADDRESS),
      base::UTF8ToUTF16("\xE8\x89\xBE\xE4\xB8\xBD\xE6\xA3\xAE@example.com"));
}

TEST_F(CollectUserDataActionTest, UserDataComplete_Contact) {
  UserData user_data;
  CollectUserDataOptions options;
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  user_data.contact_profile = std::make_unique<autofill::AutofillProfile>(
      base::GenerateGUID(), kFakeUrl);
  options.request_payer_email = true;
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  user_data.contact_profile->SetRawInfo(
      autofill::ServerFieldType::EMAIL_ADDRESS,
      base::UTF8ToUTF16("joedoe@example.com"));
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  options.request_payer_name = true;
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  user_data.contact_profile->SetRawInfo(autofill::ServerFieldType::NAME_FULL,
                                        base::UTF8ToUTF16("Joe Doe"));
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  options.request_payer_phone = true;
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  user_data.contact_profile->SetRawInfo(
      autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER,
      base::UTF8ToUTF16("+1 23 456 789 01"));
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, options));
}

TEST_F(CollectUserDataActionTest, UserDataComplete_Payment) {
  UserData user_data;
  CollectUserDataOptions options;

  options.request_payment_method = true;
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  // Valid credit card, but no billing address.
  user_data.card =
      std::make_unique<autofill::CreditCard>(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetCreditCardInfo(user_data.card.get(), "Marion Mitchell",
                                    "4111 1111 1111 1111", "01", "2020",
                                    /* billing_address_id = */ "");
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  // Incomplete billing address.
  user_data.billing_address = std::make_unique<autofill::AutofillProfile>(
      base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(user_data.billing_address.get(), "Marion",
                                 "Mitchell", "Morrison", "marion@me.xyz", "Fox",
                                 "123 Zoo St.", "unit 5", "Hollywood", "CA",
                                 /* zipcode = */ "", "US", "16505678910");
  user_data.card->set_billing_address_id(user_data.billing_address->guid());
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  user_data.billing_address->SetRawInfo(autofill::ADDRESS_HOME_ZIP,
                                        base::UTF8ToUTF16("91601"));
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  // Zip code is optional in Argentinian address.
  user_data.billing_address->SetRawInfo(autofill::ADDRESS_HOME_ZIP,
                                        base::UTF8ToUTF16(""));
  user_data.billing_address->SetRawInfo(autofill::ADDRESS_HOME_COUNTRY,
                                        base::UTF8ToUTF16("AR"));
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  options.require_billing_postal_code = true;
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  user_data.billing_address->SetRawInfo(autofill::ADDRESS_HOME_ZIP,
                                        base::UTF8ToUTF16("B1675"));
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, options));
}

TEST_F(CollectUserDataActionTest, UserDataComplete_Terms) {
  UserData user_data;
  CollectUserDataOptions options;
  options.accept_terms_and_conditions_text.assign("Accept T&C");
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  user_data.terms_and_conditions = REQUIRES_REVIEW;
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  user_data.terms_and_conditions = ACCEPTED;
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, options));
}

TEST_F(CollectUserDataActionTest, UserDataComplete_Login) {
  UserData user_data;
  CollectUserDataOptions options;
  options.request_login_choice = true;
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  user_data.login_choice_identifier.assign("1");
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, options));
}

TEST_F(CollectUserDataActionTest, UserDataComplete_ShippingAddress) {
  UserData user_data;
  CollectUserDataOptions options;
  options.request_shipping = true;
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  // Incomplete address.
  user_data.shipping_address = std::make_unique<autofill::AutofillProfile>(
      base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(user_data.shipping_address.get(), "Marion",
                                 "Mitchell", "Morrison", "marion@me.xyz", "Fox",
                                 "123 Zoo St.", "unit 5", "Hollywood", "CA",
                                 /* zipcode = */ "", "US", "16505678910");
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  user_data.shipping_address->SetRawInfo(autofill::ADDRESS_HOME_ZIP,
                                         base::UTF8ToUTF16("91601"));
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, options));
}

TEST_F(CollectUserDataActionTest, UserDataComplete_DateTimeRange) {
  UserData user_data;
  CollectUserDataOptions options;
  options.request_date_time_range = true;
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  SetDateTimeProto(&user_data.date_time_range_start, 2019, 12, 31, 10, 30, 0);
  SetDateTimeProto(&user_data.date_time_range_end, 2019, 1, 28, 16, 0, 0);

  // Start date not before end date.
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  user_data.date_time_range_end.mutable_date()->set_year(2020);
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, options));
}

TEST_F(CollectUserDataActionTest, SelectDateTimeRange) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  SetRequiredTermsFields(collect_user_data_proto);
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* date_time_proto = collect_user_data_proto->mutable_date_time_range();
  SetDateTimeProto(date_time_proto->mutable_start(), 2019, 10, 21, 8, 0, 0);
  SetDateTimeProto(date_time_proto->mutable_end(), 2019, 11, 5, 16, 0, 0);
  SetDateTimeProto(date_time_proto->mutable_min(), 2019, 11, 5, 16, 0, 0);
  SetDateTimeProto(date_time_proto->mutable_max(), 2020, 11, 5, 16, 0, 0);
  date_time_proto->set_start_label("Pick up");
  date_time_proto->set_end_label("Return");

  DateTimeProto actual_pickup_time;
  DateTimeProto actual_return_time;
  SetDateTimeProto(&actual_pickup_time, 2019, 10, 21, 7, 0, 0);
  SetDateTimeProto(&actual_return_time, 2019, 10, 25, 19, 0, 0);

  ON_CALL(mock_action_delegate_, CollectUserData(_, _))
      .WillByDefault(Invoke(
          [&](std::unique_ptr<CollectUserDataOptions> collect_user_data_options,
              std::unique_ptr<UserData> user_data) {
            user_data->succeed = true;
            user_data->date_time_range_start = actual_pickup_time;
            user_data->date_time_range_end = actual_return_time;
            std::move(collect_user_data_options->confirm_callback)
                .Run(std::move(user_data));
          }));

  EXPECT_CALL(
      callback_,
      Run(Pointee(
          AllOf(Property(&ProcessedActionProto::status, ACTION_APPLIED),
                Property(&ProcessedActionProto::collect_user_data_result,
                         Property(&CollectUserDataResultProto::date_time_start,
                                  EqualsProto(actual_pickup_time))),
                Property(&ProcessedActionProto::collect_user_data_result,
                         Property(&CollectUserDataResultProto::date_time_end,
                                  EqualsProto(actual_return_time)))))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, StaticSectionValid) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  SetRequiredTermsFields(collect_user_data_proto);
  collect_user_data_proto->set_request_terms_and_conditions(false);
  ON_CALL(mock_action_delegate_, CollectUserData(_, _))
      .WillByDefault(Invoke(
          [](std::unique_ptr<CollectUserDataOptions> collect_user_data_options,
             std::unique_ptr<UserData> user_data) {
            user_data->succeed = true;
            std::move(collect_user_data_options->confirm_callback)
                .Run(std::move(user_data));
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
  SetRequiredTermsFields(collect_user_data_proto);
  collect_user_data_proto->set_request_terms_and_conditions(false);
  ON_CALL(mock_action_delegate_, CollectUserData(_, _))
      .WillByDefault(Invoke(
          [](std::unique_ptr<CollectUserDataOptions> collect_user_data_options,
             std::unique_ptr<UserData> user_data) {
            user_data->succeed = true;
            std::move(collect_user_data_options->confirm_callback)
                .Run(std::move(user_data));
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

TEST_F(CollectUserDataActionTest, TextInputSectionWritesToClientMemory) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  SetRequiredTermsFields(collect_user_data_proto);
  collect_user_data_proto->set_request_terms_and_conditions(false);
  ON_CALL(mock_action_delegate_, CollectUserData(_, _))
      .WillByDefault(Invoke(
          [](std::unique_ptr<CollectUserDataOptions> collect_user_data_options,
             std::unique_ptr<UserData> user_data) {
            user_data->succeed = true;
            user_data->additional_values_to_store["key2"] = "modified";
            std::move(collect_user_data_options->confirm_callback)
                .Run(std::move(user_data));
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

  EXPECT_FALSE(client_memory_.has_additional_value("key1"));
  EXPECT_FALSE(client_memory_.has_additional_value("key2"));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  action.ProcessAction(callback_.Get());
  EXPECT_EQ(*client_memory_.additional_value("key1"), "initial");
  EXPECT_EQ(*client_memory_.additional_value("key2"), "modified");
}

TEST_F(CollectUserDataActionTest, AllowedBasicCardNetworks) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  SetRequiredTermsFields(collect_user_data_proto);
  collect_user_data_proto->set_request_terms_and_conditions(false);

  std::string kSupportedBasicCardNetworks[] = {"amex", "diners",   "discover",
                                               "elo",  "jcb",      "mastercard",
                                               "mir",  "unionpay", "visa"};

  for (const auto& network : kSupportedBasicCardNetworks) {
    *collect_user_data_proto->add_supported_basic_card_networks() = network;
  }

  ON_CALL(mock_action_delegate_, CollectUserData(_, _))
      .WillByDefault(Invoke(
          [](std::unique_ptr<CollectUserDataOptions> collect_user_data_options,
             std::unique_ptr<UserData> user_data) {
            user_data->succeed = true;

            user_data->billing_address =
                std::make_unique<autofill::AutofillProfile>(
                    base::GenerateGUID(), kFakeUrl);
            autofill::test::SetProfileInfo(
                user_data->billing_address.get(), "Marion", "Mitchell",
                "Morrison", "marion@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                "Hollywood", "CA", "96043", "US", "16505678910");

            user_data->card = std::make_unique<autofill::CreditCard>(
                base::GenerateGUID(), kFakeUrl);
            autofill::test::SetCreditCardInfo(
                user_data->card.get(), "Marion Mitchell", "4111 1111 1111 1111",
                "01", "2020", user_data->billing_address->guid());

            std::move(collect_user_data_options->confirm_callback)
                .Run(std::move(user_data));
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
  SetRequiredTermsFields(collect_user_data_proto);
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

TEST_F(CollectUserDataActionTest, SortsCompleteProfilesAlphabetically) {
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

  autofill::AutofillProfile profile_unicode;
  autofill::test::SetProfileInfo(&profile_unicode,
                                 "\xC3\x85"
                                 "dam",
                                 "", "West", "aedam.west@gmail.com", "", "", "",
                                 "", "", "", "", "");

  // Specify profiles in reverse order to force sorting.
  std::vector<autofill::AutofillProfile*> profiles(
      {&profile_unicode, &profile_b, &profile_a});
  ON_CALL(mock_personal_data_manager_, GetProfiles)
      .WillByDefault(Return(profiles));

  ON_CALL(mock_action_delegate_, CollectUserData(_, _))
      .WillByDefault(Invoke(
          [=](std::unique_ptr<CollectUserDataOptions> collect_user_data_options,
              std::unique_ptr<UserData> user_data) {
            user_data->succeed = true;

            user_data->contact_profile =
                std::make_unique<autofill::AutofillProfile>(profile_a);

            EXPECT_THAT(user_data->available_profiles, SizeIs(profiles.size()));
            EXPECT_EQ(user_data->available_profiles[0]->Compare(profile_a), 0);
            EXPECT_EQ(user_data->available_profiles[1]->Compare(profile_b), 0);
            EXPECT_EQ(
                user_data->available_profiles[2]->Compare(profile_unicode), 0);

            std::move(collect_user_data_options->confirm_callback)
                .Run(std::move(user_data));
          }));

  ActionProto action_proto;
  auto* user_data = action_proto.mutable_collect_user_data();
  SetRequiredTermsFields(user_data);
  auto* contact_details = user_data->mutable_contact_details();
  contact_details->set_request_payer_name(true);
  contact_details->set_request_payer_email(true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, SortsProfilesByCompleteness) {
  ON_CALL(mock_personal_data_manager_, IsAutofillProfileEnabled)
      .WillByDefault(Return(true));

  autofill::AutofillProfile profile_complete;
  autofill::test::SetProfileInfo(
      &profile_complete, "Berta", "", "West", "berta.west@gmail.com", "",
      "Baker Street 221b", "", "London", "", "WC2N 5DU", "UK", "+44");

  autofill::AutofillProfile profile_incomplete;
  autofill::test::SetProfileInfo(&profile_incomplete, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "+41");

  // Specify profiles in reverse order to force sorting.
  std::vector<autofill::AutofillProfile*> profiles(
      {&profile_incomplete, &profile_complete});
  ON_CALL(mock_personal_data_manager_, GetProfiles)
      .WillByDefault(Return(profiles));

  ON_CALL(mock_action_delegate_, CollectUserData(_, _))
      .WillByDefault(Invoke(
          [=](std::unique_ptr<CollectUserDataOptions> collect_user_data_options,
              std::unique_ptr<UserData> user_data) {
            user_data->succeed = true;

            user_data->contact_profile =
                std::make_unique<autofill::AutofillProfile>(profile_complete);
            user_data->shipping_address =
                std::make_unique<autofill::AutofillProfile>(profile_complete);

            EXPECT_THAT(user_data->available_profiles, SizeIs(2));
            EXPECT_EQ(
                user_data->available_profiles[0]->Compare(profile_complete), 0);
            EXPECT_EQ(
                user_data->available_profiles[1]->Compare(profile_incomplete),
                0);

            std::move(collect_user_data_options->confirm_callback)
                .Run(std::move(user_data));
          }));

  ActionProto action_proto;
  auto* user_data = action_proto.mutable_collect_user_data();
  SetRequiredTermsFields(user_data);
  user_data->set_shipping_address_name("Address");
  auto* contact_details = user_data->mutable_contact_details();
  contact_details->set_request_payer_name(true);
  contact_details->set_request_payer_email(true);
  contact_details->set_request_payer_phone(true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

}  // namespace
}  // namespace autofill_assistant
