// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/update_client_settings_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::_;
using ::testing::Property;
using ::testing::Return;

class UpdateClientSettingsActionTest : public testing::Test {
 public:
  UpdateClientSettingsActionTest() {}

  void SetUp() override {
    ON_CALL(mock_action_delegate_, SetClientSettings(_))
        .WillByDefault(Return());
  }

 protected:
  void Run(const UpdateClientSettingsProto& proto) {
    ActionProto action_proto;
    *action_proto.mutable_update_client_settings() = proto;
    UpdateClientSettingsAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  MockActionDelegate mock_action_delegate_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
};

TEST_F(UpdateClientSettingsActionTest, NoStringsNoLocaleIsValid) {
  UpdateClientSettingsProto proto;
  proto.mutable_client_settings()->set_periodic_script_check_interval_ms(10);
  EXPECT_CALL(mock_action_delegate_, SetClientSettings(_)).Times(1);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run(proto);
}

TEST_F(UpdateClientSettingsActionTest,
       CompleteStringsSetWithNoLocaleAreInvalid) {
  UpdateClientSettingsProto proto;
  for (int i = 0; i <= ClientSettingsProto::DisplayStringId_MAX; i++) {
    ClientSettingsProto::DisplayString* display_string =
        proto.mutable_client_settings()->add_display_strings();
    display_string->set_id(
        static_cast<ClientSettingsProto::DisplayStringId>(i));
    display_string->set_value("test");
  }
  EXPECT_CALL(mock_action_delegate_, SetClientSettings(_)).Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  Run(proto);
}

TEST_F(UpdateClientSettingsActionTest, LocaleWithNoStringsIsInvalid) {
  UpdateClientSettingsProto proto;
  proto.mutable_client_settings()->set_display_strings_locale("en-US");
  EXPECT_CALL(mock_action_delegate_, SetClientSettings(_)).Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  Run(proto);
}

TEST_F(UpdateClientSettingsActionTest, LocaleWithOneLessStringIsInvalid) {
  UpdateClientSettingsProto proto;
  // Including UNSPECIFIED enum value as well, and should be ignored.
  for (int i = 0; i < ClientSettingsProto::DisplayStringId_MAX; i++) {
    ClientSettingsProto::DisplayString* display_string =
        proto.mutable_client_settings()->add_display_strings();
    display_string->set_id(
        static_cast<ClientSettingsProto::DisplayStringId>(i));
    display_string->set_value("test");
  }
  proto.mutable_client_settings()->set_display_strings_locale("en-US");
  EXPECT_CALL(mock_action_delegate_, SetClientSettings(_)).Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  Run(proto);
}

TEST_F(UpdateClientSettingsActionTest, LocaleWithCompleteStringsSetIsValid) {
  UpdateClientSettingsProto proto;
  // Skipping UNSPECIFIED enum value.
  for (int i = 1; i <= ClientSettingsProto::DisplayStringId_MAX; i++) {
    ClientSettingsProto::DisplayString* display_string =
        proto.mutable_client_settings()->add_display_strings();
    display_string->set_id(
        static_cast<ClientSettingsProto::DisplayStringId>(i));
    display_string->set_value("test");
  }
  proto.mutable_client_settings()->set_display_strings_locale("en-US");
  EXPECT_CALL(mock_action_delegate_, SetClientSettings(_)).Times(1);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run(proto);
}

}  // namespace
}  // namespace autofill_assistant
