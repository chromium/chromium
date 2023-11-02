// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/external_action.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/actions/wait_for_dom_test_base.h"
#include "components/autofill_assistant/browser/mock_user_model.h"
#include "components/autofill_assistant/browser/public/external_action.pb.h"
#include "components/autofill_assistant/browser/public/external_action_util.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

constexpr char kProfileName[] = "SHIPPING";

using ::autofill::ServerFieldType;
using ::autofill::structured_address::VerificationStatus;
using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::UnorderedElementsAre;
using ::testing::WithArgs;

class ExternalActionTest : public WaitForDomTestBase {
 public:
  ExternalActionTest() = default;

  void SetUp() override {
    mock_user_model_ = std::make_unique<MockUserModel>();
    ON_CALL(mock_action_delegate_, GetUserModel)
        .WillByDefault(Return(GetMockUserModel()));
    user_data_ = std::make_unique<UserData>();
    ON_CALL(mock_action_delegate_, GetMutableUserData)
        .WillByDefault(Return(user_data_.get()));
    ON_CALL(mock_action_delegate_, GetLocale).WillByDefault(Return(locale_));
    base::Time fake_now;
    if (base::Time::FromUTCString("2022-07-01 00:00:00", &fake_now))
      test_clock_.SetNow(fake_now);
  }

 protected:
  void Run() {
    ON_CALL(mock_action_delegate_, SupportsExternalActions)
        .WillByDefault(Return(true));

    ActionProto action_proto;
    *action_proto.mutable_external_action() = proto_;
    action_ =
        std::make_unique<ExternalAction>(&mock_action_delegate_, action_proto);
    action_->ProcessAction(callback_.Get());
  }

  MockUserModel* GetMockUserModel() { return mock_user_model_.get(); }

  base::MockCallback<Action::ProcessActionCallback> callback_;
  ExternalActionProto proto_;
  std::unique_ptr<ExternalAction> action_;

 private:
  const std::string locale_ = "en-US";
  std::unique_ptr<UserData> user_data_;
  std::unique_ptr<MockUserModel> mock_user_model_;
  autofill::TestAutofillClock test_clock_;
};

external::Result MakeResult(bool success) {
  external::Result result;
  result.set_success(success);

  external::ResultInfo dummy_result_info;
  *result.mutable_result_info() = dummy_result_info;

  return result;
}

TEST_F(ExternalActionTest, Success) {
  proto_.mutable_info();

  EXPECT_CALL(mock_action_delegate_, RequestExternalAction)
      .WillOnce(RunOnceCallback<2>(MakeResult(/* success= */ true)));

  std::unique_ptr<ProcessedActionProto> returned_processed_action_proto;
  EXPECT_CALL(callback_, Run)
      .WillOnce(
          [&returned_processed_action_proto](
              std::unique_ptr<ProcessedActionProto> processed_action_proto) {
            returned_processed_action_proto = std::move(processed_action_proto);
          });
  Run();
  EXPECT_THAT(returned_processed_action_proto->status(), Eq(ACTION_APPLIED));
  ASSERT_TRUE(returned_processed_action_proto->external_action_result()
                  .has_result_info());
}

TEST_F(ExternalActionTest, ExternalFailure) {
  proto_.mutable_info();

  std::unique_ptr<ProcessedActionProto> returned_processed_action_proto;
  EXPECT_CALL(callback_, Run)
      .WillOnce(
          [&returned_processed_action_proto](
              std::unique_ptr<ProcessedActionProto> processed_action_proto) {
            returned_processed_action_proto = std::move(processed_action_proto);
          });
  EXPECT_CALL(mock_action_delegate_, RequestExternalAction)
      .WillOnce(RunOnceCallback<2>(MakeResult(/* success= */ false)));
  Run();
  EXPECT_THAT(returned_processed_action_proto->status(),
              Eq(UNKNOWN_ACTION_STATUS));
  EXPECT_TRUE(returned_processed_action_proto->has_external_action_result());
  ASSERT_TRUE(returned_processed_action_proto->external_action_result()
                  .has_result_info());
}

TEST_F(ExternalActionTest, FailsIfProtoExtensionInfoNotSet) {
  EXPECT_CALL(mock_action_delegate_, RequestExternalAction).Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  Run();
}

TEST_F(ExternalActionTest, FailsIfDelegateDoesNotSupportExternalActions) {
  proto_.mutable_info();
  EXPECT_CALL(mock_action_delegate_, SupportsExternalActions())
      .WillOnce(Return(false));
  EXPECT_CALL(mock_action_delegate_, RequestExternalAction).Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  Run();
}

TEST_F(ExternalActionTest, ExternalActionWithInterrupts) {
  proto_.mutable_info();
  proto_.set_allow_interrupt(true);

  EXPECT_CALL(mock_action_delegate_, RequestExternalAction)
      .WillOnce(
          [](const ExternalActionProto& external_action,
             base::OnceCallback<void(ExternalActionDelegate::DomUpdateCallback)>
                 start_dom_checks_callback,
             base::OnceCallback<void(const external::Result& result)>
                 end_action_callback) {
            std::move(start_dom_checks_callback).Run(base::DoNothing());
            std::move(end_action_callback).Run(MakeResult(/* success= */ true));
          });
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
  // The action should end at the next WaitForDom notification.
  task_env_.FastForwardBy(base::Seconds(1));
}

TEST_F(ExternalActionTest, ExternalActionWithoutInterrupts) {
  proto_.mutable_info();
  proto_.set_allow_interrupt(false);

  EXPECT_CALL(mock_action_delegate_, RequestExternalAction)
      .WillOnce(
          [](const ExternalActionProto& external_action,
             base::OnceCallback<void(ExternalActionDelegate::DomUpdateCallback)>
                 start_dom_checks_callback,
             base::OnceCallback<void(const external::Result& result)>
                 end_action_callback) {
            std::move(start_dom_checks_callback).Run(base::DoNothing());
            std::move(end_action_callback).Run(MakeResult(/* success= */ true));
          });
  EXPECT_CALL(mock_action_delegate_, WaitForDom).Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(ExternalActionTest, ExternalActionWithSelectedProfileAndCreditCard) {
  proto_.mutable_info();
  proto_.set_allow_interrupt(false);

  // Credit card expiration date
  base::Time credit_card_exp_date =
      autofill::AutofillClock::Now() + base::Days(31);
  base::Time::Exploded credit_card_exp_date_exploded;
  credit_card_exp_date.UTCExplode(&credit_card_exp_date_exploded);

  std::u16string credit_card_exp_month = base::UTF8ToUTF16(
      (credit_card_exp_date_exploded.month < 10 ? "0" : "") +
      base::NumberToString(credit_card_exp_date_exploded.month));
  std::u16string credit_card_exp_year_4_digits = base::UTF8ToUTF16(
      base::NumberToString(credit_card_exp_date_exploded.year));
  std::u16string credit_card_exp_year_2_digits =
      credit_card_exp_year_4_digits.substr(2, 2);
  std::u16string credit_card_exp_month_year_4_digits =
      credit_card_exp_month + u"/" + credit_card_exp_year_4_digits;
  std::u16string credit_card_exp_month_year_2_digits =
      credit_card_exp_month + u"/" + credit_card_exp_year_2_digits;

  // Result proto
  external::Result result = MakeResult(/* success= */ true);

  // Original profile
  std::unique_ptr<autofill::AutofillProfile> original_autofill_profile =
      std::make_unique<autofill::AutofillProfile>();
  {
    original_autofill_profile->SetRawInfoWithVerificationStatus(
        ServerFieldType::NAME_FIRST, u"First", VerificationStatus::kObserved);
    original_autofill_profile->SetRawInfoWithVerificationStatus(
        ServerFieldType::NAME_LAST, u"Last", VerificationStatus::kObserved);
    original_autofill_profile->SetRawInfoWithVerificationStatus(
        ServerFieldType::NAME_FULL, u"First Last",
        VerificationStatus::kObserved);
    original_autofill_profile->SetRawInfoWithVerificationStatus(
        ServerFieldType::EMAIL_ADDRESS, u"first.last@example.com",
        VerificationStatus::kObserved);
    original_autofill_profile->SetRawInfoWithVerificationStatus(
        ServerFieldType::PHONE_HOME_NUMBER, u"5555555",
        VerificationStatus::kObserved);
    original_autofill_profile->SetRawInfoWithVerificationStatus(
        ServerFieldType::PHONE_HOME_CITY_CODE, u"919",
        VerificationStatus::kObserved);
    original_autofill_profile->SetRawInfoWithVerificationStatus(
        ServerFieldType::PHONE_HOME_CITY_AND_NUMBER, u"9195555555",
        VerificationStatus::kObserved);
    original_autofill_profile->SetRawInfoWithVerificationStatus(
        ServerFieldType::PHONE_HOME_WHOLE_NUMBER, u"9195555555",
        VerificationStatus::kObserved);
    original_autofill_profile->SetRawInfoWithVerificationStatus(
        ServerFieldType::ADDRESS_HOME_LINE1, u"100 Some Way",
        VerificationStatus::kObserved);
    original_autofill_profile->SetRawInfoWithVerificationStatus(
        ServerFieldType::ADDRESS_HOME_CITY, u"Knighttown",
        VerificationStatus::kObserved);
    original_autofill_profile->SetRawInfoWithVerificationStatus(
        ServerFieldType::ADDRESS_HOME_STATE, u"NC",
        VerificationStatus::kObserved);
    original_autofill_profile->SetRawInfoWithVerificationStatus(
        ServerFieldType::ADDRESS_HOME_ZIP, u"12345",
        VerificationStatus::kObserved);
    original_autofill_profile->SetRawInfoWithVerificationStatus(
        ServerFieldType::ADDRESS_HOME_COUNTRY, u"UNITED STATES",
        VerificationStatus::kObserved);
    original_autofill_profile->SetRawInfoWithVerificationStatus(
        ServerFieldType::ADDRESS_HOME_STREET_ADDRESS, u"100 Some Way",
        VerificationStatus::kObserved);
    original_autofill_profile->SetRawInfoWithVerificationStatus(
        ServerFieldType::ADDRESS_HOME_STREET_NAME, u"Some Way",
        VerificationStatus::kObserved);
    original_autofill_profile->SetRawInfoWithVerificationStatus(
        ServerFieldType::ADDRESS_HOME_HOUSE_NUMBER, u"100",
        VerificationStatus::kObserved);
    original_autofill_profile->SetRawInfoWithVerificationStatus(
        ServerFieldType::NAME_LAST_SECOND, u"Last",
        VerificationStatus::kObserved);
    original_autofill_profile->SetRawInfoWithVerificationStatus(
        ServerFieldType::PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX, u"919",
        VerificationStatus::kObserved);
    original_autofill_profile->SetRawInfoWithVerificationStatus(
        ServerFieldType::PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX,
        u"9195555555", VerificationStatus::kObserved);
    original_autofill_profile->SetRawInfoWithVerificationStatus(
        ServerFieldType::PHONE_HOME_NUMBER_PREFIX, u"555",
        VerificationStatus::kObserved);
    original_autofill_profile->SetRawInfoWithVerificationStatus(
        ServerFieldType::PHONE_HOME_NUMBER_SUFFIX, u"5555",
        VerificationStatus::kObserved);
  }

  // Original card
  std::unique_ptr<autofill::CreditCard> original_card =
      std::make_unique<autofill::CreditCard>();
  {
    original_card->SetRawInfoWithVerificationStatus(
        ServerFieldType::CREDIT_CARD_NAME_FULL, u"First Last",
        VerificationStatus::kObserved);
    original_card->SetRawInfoWithVerificationStatus(
        ServerFieldType::CREDIT_CARD_NUMBER, u"4111111111111111",
        VerificationStatus::kObserved);
    original_card->SetRawInfoWithVerificationStatus(
        ServerFieldType::CREDIT_CARD_EXP_MONTH, credit_card_exp_month,
        VerificationStatus::kObserved);
    original_card->SetRawInfoWithVerificationStatus(
        ServerFieldType::CREDIT_CARD_EXP_2_DIGIT_YEAR,
        credit_card_exp_year_2_digits, VerificationStatus::kObserved);
    original_card->SetRawInfoWithVerificationStatus(
        ServerFieldType::CREDIT_CARD_EXP_4_DIGIT_YEAR,
        credit_card_exp_year_4_digits, VerificationStatus::kObserved);
    original_card->SetRawInfoWithVerificationStatus(
        ServerFieldType::CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
        credit_card_exp_month_year_2_digits, VerificationStatus::kObserved);
    original_card->SetRawInfoWithVerificationStatus(
        ServerFieldType::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
        credit_card_exp_month_year_4_digits, VerificationStatus::kObserved);
    original_card->SetRawInfoWithVerificationStatus(
        ServerFieldType::CREDIT_CARD_TYPE, u"Visa",
        VerificationStatus::kObserved);
    original_card->SetRawInfoWithVerificationStatus(
        ServerFieldType::CREDIT_CARD_NAME_FIRST, u"First",
        VerificationStatus::kObserved);
    original_card->SetRawInfoWithVerificationStatus(
        ServerFieldType::CREDIT_CARD_NAME_LAST, u"Last",
        VerificationStatus::kObserved);
    original_card->set_origin("Chrome settings");
    original_card->set_record_type(
        autofill::CreditCard::RecordType::LOCAL_CARD);
    original_card->set_instrument_id(0);
  }

  // Profile proto
  external::ProfileProto profile_proto =
      CreateProfileProto(*original_autofill_profile);
  (result.mutable_selected_profiles())->insert({kProfileName, profile_proto});

  // Card proto
  std::unique_ptr<external::CreditCardProto> card_proto(
      new external::CreditCardProto(CreateCreditCardProto(*original_card)));
  result.set_allocated_selected_credit_card(card_proto.release());

  EXPECT_CALL(mock_action_delegate_, RequestExternalAction)
      .WillOnce(
          [&result](const ExternalActionProto& external_action,
                    base::OnceCallback<void(
                        ExternalActionDelegate::DomUpdateCallback)>
                        start_dom_checks_callback,
                    base::OnceCallback<void(const external::Result& result)>
                        end_action_callback) {
            std::move(start_dom_checks_callback).Run(base::DoNothing());
            std::move(end_action_callback).Run(result);
          });
  EXPECT_CALL(mock_action_delegate_, WaitForDom).Times(0);
  std::unique_ptr<autofill::AutofillProfile> autofill_profile;
  EXPECT_CALL(*GetMockUserModel(),
              SetSelectedAutofillProfile(kProfileName, ::testing::NotNull(), _))
      .WillOnce([&autofill_profile](
                    auto, std::unique_ptr<autofill::AutofillProfile> ap, auto) {
        autofill_profile = std::move(ap);
      });
  std::unique_ptr<autofill::CreditCard> card;
  EXPECT_CALL(*GetMockUserModel(),
              SetSelectedCreditCard(::testing::NotNull(), _))
      .WillOnce([&card](std::unique_ptr<autofill::CreditCard> cc, auto) {
        card = std::move(cc);
      });
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  Run();

  // Verify profile and card data
  EXPECT_EQ(*original_autofill_profile, *autofill_profile);
  EXPECT_EQ(*original_card, *card);
}

TEST_F(ExternalActionTest, DoesNotStartWaitForDomIfDomChecksAreNotRequested) {
  proto_.mutable_info();
  proto_.set_allow_interrupt(true);

  EXPECT_CALL(mock_action_delegate_, RequestExternalAction)
      .WillOnce(
          [](const ExternalActionProto& external_action,
             base::OnceCallback<void(ExternalActionDelegate::DomUpdateCallback)>
                 start_dom_checks_callback,
             base::OnceCallback<void(const external::Result& result)>
                 end_action_callback) {
            // We call the |end_action_callback| without calling
            // |start_dom_checks_callback|.
            std::move(end_action_callback).Run(MakeResult(/* success= */ true));
          });
  EXPECT_CALL(mock_action_delegate_, WaitForDom).Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(ExternalActionTest, ExternalActionWithDomChecks) {
  proto_.mutable_info();
  ExternalActionProto::ExternalCondition condition;
  condition.set_id(55);
  *condition.mutable_element_condition()->mutable_match() =
      ToSelectorProto("element");
  *proto_.add_conditions() = condition;

  base::MockCallback<ExternalActionDelegate::DomUpdateCallback>
      dom_update_callback;

  EXPECT_CALL(mock_action_delegate_, RequestExternalAction)
      .WillOnce([&dom_update_callback](
                    const ExternalActionProto& external_action,
                    base::OnceCallback<void(
                        ExternalActionDelegate::DomUpdateCallback)>
                        start_dom_checks_callback,
                    base::OnceCallback<void(const external::Result& result)>
                        end_action_callback) {
        std::move(start_dom_checks_callback).Run(dom_update_callback.Get());
        std::move(end_action_callback).Run(MakeResult(/* success= */ true));
      });

  EXPECT_CALL(
      dom_update_callback,
      Run(Property(
          &external::ElementConditionsUpdate::results,
          ElementsAre(AllOf(
              Property(&external::ElementConditionsUpdate::ConditionResult::id,
                       55),
              Property(&external::ElementConditionsUpdate::ConditionResult::
                           satisfied,
                       false))))));
  Run();

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  // The action should end at the next WaitForDom notification.
  task_env_.FastForwardBy(base::Seconds(1));
}

TEST_F(ExternalActionTest, DomChecksOnlyUpdateOnChange) {
  proto_.mutable_info();
  ExternalActionProto::ExternalCondition changing_condition;
  changing_condition.set_id(55);
  *changing_condition.mutable_element_condition()->mutable_match() =
      ToSelectorProto("changing_condition");
  ExternalActionProto::ExternalCondition unchanging_condition;
  unchanging_condition.set_id(9);
  *unchanging_condition.mutable_element_condition()->mutable_match() =
      ToSelectorProto("unchanging_condition");
  *proto_.add_conditions() = changing_condition;
  *proto_.add_conditions() = unchanging_condition;

  base::MockCallback<ExternalActionDelegate::DomUpdateCallback>
      dom_update_callback;

  EXPECT_CALL(mock_action_delegate_, RequestExternalAction)
      .WillOnce([&dom_update_callback](
                    const ExternalActionProto& external_action,
                    base::OnceCallback<void(
                        ExternalActionDelegate::DomUpdateCallback)>
                        start_dom_checks_callback,
                    base::OnceCallback<void(const external::Result& result)>
                        end_action_callback) {
        std::move(start_dom_checks_callback).Run(dom_update_callback.Get());
      });

  // For the first rounds of checks, all elements should be in the notification.
  // Note that the |mock_web_controller_| reports an element as missing by
  // default in the fixture.
  EXPECT_CALL(
      dom_update_callback,
      Run(Property(
          &external::ElementConditionsUpdate::results,
          UnorderedElementsAre(
              AllOf(Property(
                        &external::ElementConditionsUpdate::ConditionResult::id,
                        55),
                    Property(&external::ElementConditionsUpdate::
                                 ConditionResult::satisfied,
                             false)),
              AllOf(Property(
                        &external::ElementConditionsUpdate::ConditionResult::id,
                        9),
                    Property(&external::ElementConditionsUpdate::
                                 ConditionResult::satisfied,
                             false))))));

  Run();

  // For the second rounds of checks, we simulate the |changing_condition|
  // changing to being satisfied and |unchanging_condition| remaining
  // unsatisfied.
  EXPECT_CALL(mock_web_controller_,
              FindElement(Selector({"changing_condition"}), _, _))
      .WillOnce(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinderResult>());
      }));
  EXPECT_CALL(mock_web_controller_,
              FindElement(Selector({"unchanging_condition"}), _, _))
      .WillOnce(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(ClientStatus(ELEMENT_RESOLUTION_FAILED),
                                std::make_unique<ElementFinderResult>());
      }));

  // The notification should now only contain an entry for |changed_condition|.
  EXPECT_CALL(
      dom_update_callback,
      Run(Property(
          &external::ElementConditionsUpdate::results,
          UnorderedElementsAre(AllOf(
              Property(&external::ElementConditionsUpdate::ConditionResult::id,
                       55),
              Property(&external::ElementConditionsUpdate::ConditionResult::
                           satisfied,
                       true))))));
  task_env_.FastForwardBy(base::Seconds(1));

  // We keep the same state as the last roundtrip.
  EXPECT_CALL(mock_web_controller_,
              FindElement(Selector({"changing_condition"}), _, _))
      .WillOnce(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinderResult>());
      }));
  EXPECT_CALL(mock_web_controller_,
              FindElement(Selector({"unchanging_condition"}), _, _))
      .WillOnce(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(ClientStatus(ELEMENT_RESOLUTION_FAILED),
                                std::make_unique<ElementFinderResult>());
      }));
  // Since there were no changes, no notification is sent.
  EXPECT_CALL(dom_update_callback, Run(_)).Times(0);
  task_env_.FastForwardBy(base::Seconds(1));
}

TEST_F(ExternalActionTest, WaitForDomFailure) {
  proto_.mutable_info();
  proto_.set_allow_interrupt(true);

  EXPECT_CALL(mock_action_delegate_, RequestExternalAction)
      .WillOnce(
          [](const ExternalActionProto& external_action,
             base::OnceCallback<void(ExternalActionDelegate::DomUpdateCallback)>
                 start_dom_checks_callback,
             base::OnceCallback<void(const external::Result& result)>
                 end_action_callback) {
            std::move(start_dom_checks_callback).Run(base::DoNothing());
            std::move(end_action_callback).Run(MakeResult(/* success= */ true));
          });

  // Even if the external action ended in a success, if the WaitForDom ends in
  // an error we expect the error to be reported.
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INTERRUPT_FAILED))));
  Run();
  wait_for_dom_status_ = ClientStatus(INTERRUPT_FAILED);
  // The action should end at the next WaitForDom notification.
  task_env_.FastForwardBy(base::Seconds(1));
}

}  // namespace
}  // namespace autofill_assistant
