// Copyright 2022 The Chromium Authors. All rights reserved.
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
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::autofill::ServerFieldType;
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

namespace {

constexpr char kCreditCardNumber[] = "4111111111111111";
constexpr char kProfileName[] = "SHIPPING";
constexpr char kFirstName[] = "John";
constexpr char kLastName[] = "Doe";
constexpr char kEmail[] = "jd@example.com";
constexpr char kAddressLine1[] = "Erika-Mann-Str. 33";
constexpr char kAddressCity[] = "Munich";
constexpr char kAddressZip[] = "80636";
constexpr int64_t kInstrumentId = 123;
constexpr char kServerId[] = "server id";
constexpr autofill::CreditCard::RecordType kRecordType =
    autofill::CreditCard::RecordType::LOCAL_CARD;
}  // namespace

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

  base::Time credit_card_exp_date =
      autofill::AutofillClock::Now() + base::Days(31);
  base::Time::Exploded credit_card_exp_date_exploded;
  credit_card_exp_date.UTCExplode(&credit_card_exp_date_exploded);

  // Result proto
  external::Result result = MakeResult(/* success= */ true);

  // Credit card proto
  auto credit_card_proto = std::make_unique<external::CreditCardProto>();
  (*credit_card_proto->mutable_values())[ServerFieldType::CREDIT_CARD_NUMBER] =
      kCreditCardNumber;
  (*credit_card_proto
        ->mutable_values())[ServerFieldType::CREDIT_CARD_EXP_MONTH] =
      base::NumberToString(credit_card_exp_date_exploded.month);
  (*credit_card_proto
        ->mutable_values())[ServerFieldType::CREDIT_CARD_EXP_4_DIGIT_YEAR] =
      base::NumberToString(credit_card_exp_date_exploded.year);
  credit_card_proto->set_record_type(kRecordType);
  credit_card_proto->set_instrument_id(kInstrumentId);
  credit_card_proto->set_server_id(kServerId);
  result.set_allocated_selected_credit_card(credit_card_proto.get());
  credit_card_proto.release();

  // Profile proto
  auto profile_proto = std::make_unique<external::ProfileProto>();
  (*profile_proto->mutable_values())[ServerFieldType::NAME_FIRST] = kFirstName;
  (*profile_proto->mutable_values())[ServerFieldType::NAME_LAST] = kLastName;
  (*profile_proto->mutable_values())[ServerFieldType::EMAIL_ADDRESS] = kEmail;
  (*profile_proto->mutable_values())[ServerFieldType::ADDRESS_HOME_LINE1] =
      kAddressLine1;
  (*profile_proto->mutable_values())[ServerFieldType::ADDRESS_HOME_CITY] =
      kAddressCity;
  (*profile_proto->mutable_values())[ServerFieldType::ADDRESS_HOME_ZIP] =
      kAddressZip;
  (result.mutable_selected_profiles())->insert({kProfileName, *profile_proto});

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
  std::unique_ptr<autofill::CreditCard> credit_card;
  EXPECT_CALL(*GetMockUserModel(),
              SetSelectedCreditCard(::testing::NotNull(), _))
      .WillOnce([&credit_card](std::unique_ptr<autofill::CreditCard> cc, auto) {
        credit_card = std::move(cc);
      });
  std::unique_ptr<autofill::AutofillProfile> autofill_profile;
  EXPECT_CALL(*GetMockUserModel(),
              SetSelectedAutofillProfile(kProfileName, ::testing::NotNull(), _))
      .WillOnce([&autofill_profile](
                    auto, std::unique_ptr<autofill::AutofillProfile> ap, auto) {
        autofill_profile = std::move(ap);
      });
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  Run();

  // Verify credit card data
  EXPECT_EQ(credit_card->expiration_month(),
            credit_card_exp_date_exploded.month);
  EXPECT_EQ(credit_card->expiration_year(), credit_card_exp_date_exploded.year);
  EXPECT_EQ(base::UTF16ToUTF8(credit_card->number()), kCreditCardNumber);
  EXPECT_EQ(credit_card->record_type(), kRecordType);
  EXPECT_EQ(credit_card->instrument_id(), kInstrumentId);
  EXPECT_EQ(credit_card->server_id(), kServerId);

  // Verify profile data
  EXPECT_EQ(
      base::UTF16ToUTF8(autofill_profile->GetInfo(
          ServerFieldType::NAME_FIRST, mock_action_delegate_.GetLocale())),
      kFirstName);
  EXPECT_EQ(base::UTF16ToUTF8(autofill_profile->GetInfo(
                ServerFieldType::NAME_LAST, mock_action_delegate_.GetLocale())),
            kLastName);
  EXPECT_EQ(
      base::UTF16ToUTF8(autofill_profile->GetInfo(
          ServerFieldType::EMAIL_ADDRESS, mock_action_delegate_.GetLocale())),
      kEmail);
  EXPECT_EQ(base::UTF16ToUTF8(
                autofill_profile->GetInfo(ServerFieldType::ADDRESS_HOME_LINE1,
                                          mock_action_delegate_.GetLocale())),
            kAddressLine1);
  EXPECT_EQ(base::UTF16ToUTF8(
                autofill_profile->GetInfo(ServerFieldType::ADDRESS_HOME_CITY,
                                          mock_action_delegate_.GetLocale())),
            kAddressCity);
  EXPECT_EQ(base::UTF16ToUTF8(
                autofill_profile->GetInfo(ServerFieldType::ADDRESS_HOME_ZIP,
                                          mock_action_delegate_.GetLocale())),
            kAddressZip);
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
