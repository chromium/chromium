// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/show_details_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/client_memory.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Property;
using ::testing::Return;

class ShowDetailsActionTest : public testing::Test {
 public:
  ShowDetailsActionTest() {}

  void SetUp() override {
    autofill::CountryNames::SetLocaleString("us-en");

    ON_CALL(mock_action_delegate_, GetClientMemory())
        .WillByDefault(Return(&client_memory_));
    ON_CALL(mock_action_delegate_, SetDetails(_)).WillByDefault(Return());
  }

 protected:
  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_show_details() = proto_;
    ShowDetailsAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  std::unique_ptr<autofill::CreditCard> MakeCreditCard() {
    return std::make_unique<autofill::CreditCard>();
  }

  std::unique_ptr<autofill::AutofillProfile> MakeAutofillProfile() {
    auto profile = std::make_unique<autofill::AutofillProfile>();
    autofill::test::SetProfileInfo(profile.get(), "Charles", "Hardin", "Holley",
                                   "buddy@gmail.com", "Decca", "123 Apple St.",
                                   "unit 6", "Lubbock", "Texas", "79401", "US",
                                   "23456789012");
    return profile;
  }

  ClientMemory client_memory_;
  MockActionDelegate mock_action_delegate_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  ShowDetailsProto proto_;
};

TEST_F(ShowDetailsActionTest, EmptyIsValid) {
  EXPECT_CALL(mock_action_delegate_, SetDetails(_));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(ShowDetailsActionTest, DetailsCase) {
  proto_.mutable_details();

  EXPECT_CALL(mock_action_delegate_, SetDetails(_));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(ShowDetailsActionTest, ContactDetailsCase) {
  proto_.set_contact_details("contact");
  client_memory_.set_selected_address("contact", MakeAutofillProfile());

  EXPECT_CALL(mock_action_delegate_, SetDetails(_));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(ShowDetailsActionTest, ShippingAddressCase) {
  proto_.set_shipping_address("shipping");
  client_memory_.set_selected_address("shipping", MakeAutofillProfile());

  EXPECT_CALL(mock_action_delegate_, SetDetails(_));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(ShowDetailsActionTest, CreditCardCase) {
  proto_.set_credit_card(true);
  client_memory_.set_selected_card(MakeCreditCard());

  EXPECT_CALL(mock_action_delegate_, SetDetails(_));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(ShowDetailsActionTest, CreditCardRequestedButNotAvailable) {
  proto_.set_credit_card(true);
  EXPECT_CALL(mock_action_delegate_, SetDetails(_)).Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  Run();
}

}  // namespace
}  // namespace autofill_assistant
