// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/display_strings_util.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_assistant {
namespace {

void AddDisplayStringToProto(ClientSettingsProto::DisplayStringId id,
                             const std::string str,
                             ClientSettingsProto& proto) {
  ClientSettingsProto::DisplayString* display_str = proto.add_display_strings();
  display_str->set_id(id);
  display_str->set_value(str);
}

TEST(DisplayStringsUtilTest, FallbackToChromeStringsByDefault) {
  ClientSettings client_settings;
  for (int i = 0; i < ClientSettingsProto::DisplayStringId_MAX + 1; i++) {
    switch (static_cast<ClientSettingsProto::DisplayStringId>(i)) {
      case ClientSettingsProto::UNSPECIFIED:
        EXPECT_EQ(GetDisplayStringUTF8(ClientSettingsProto::UNSPECIFIED,
                                       client_settings),
                  "");
        break;
      case ClientSettingsProto::GIVE_UP:
        EXPECT_EQ(
            GetDisplayStringUTF8(ClientSettingsProto::GIVE_UP, client_settings),
            l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_GIVE_UP));
        break;
      case ClientSettingsProto::MAYBE_GIVE_UP:
        EXPECT_EQ(
            GetDisplayStringUTF8(ClientSettingsProto::MAYBE_GIVE_UP,
                                 client_settings),
            l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_MAYBE_GIVE_UP));
        break;
      case ClientSettingsProto::DEFAULT_ERROR:
        EXPECT_EQ(
            GetDisplayStringUTF8(ClientSettingsProto::DEFAULT_ERROR,
                                 client_settings),
            l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_DEFAULT_ERROR));
        break;
      case ClientSettingsProto::PAYMENT_INFO_CONFIRM:
        EXPECT_EQ(
            GetDisplayStringUTF8(ClientSettingsProto::PAYMENT_INFO_CONFIRM,
                                 client_settings),
            l10n_util::GetStringUTF8(
                IDS_AUTOFILL_ASSISTANT_PAYMENT_INFO_CONFIRM));
        break;
      case ClientSettingsProto::CONTINUE_BUTTON:
        EXPECT_EQ(
            GetDisplayStringUTF8(ClientSettingsProto::CONTINUE_BUTTON,
                                 client_settings),
            l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_CONTINUE_BUTTON));
        break;
      case ClientSettingsProto::STOPPED:
        EXPECT_EQ(
            GetDisplayStringUTF8(ClientSettingsProto::STOPPED, client_settings),
            l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_STOPPED));
        break;
      case ClientSettingsProto::SEND_FEEDBACK:
        EXPECT_EQ(
            GetDisplayStringUTF8(ClientSettingsProto::SEND_FEEDBACK,
                                 client_settings),
            l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_SEND_FEEDBACK));
        break;
      case ClientSettingsProto::CLOSE:
        EXPECT_EQ(
            GetDisplayStringUTF8(ClientSettingsProto::CLOSE, client_settings),
            l10n_util::GetStringUTF8(IDS_CLOSE));
        break;
      case ClientSettingsProto::SETTINGS:
        EXPECT_EQ(GetDisplayStringUTF8(ClientSettingsProto::SETTINGS,
                                       client_settings),
                  l10n_util::GetStringUTF8(IDS_SETTINGS_TITLE));
        break;
      case ClientSettingsProto::UNDO:
        EXPECT_EQ(
            GetDisplayStringUTF8(ClientSettingsProto::UNDO, client_settings),
            "");
        break;
    }
  }
}

TEST(DisplayStringsUtilTest, ReturnValidDisplayString) {
  ClientSettingsProto proto;
  proto.set_display_strings_locale("en-US");
  proto.mutable_back_button_settings()->set_undo_label("undo");
  AddDisplayStringToProto(ClientSettingsProto::GIVE_UP, "give_up", proto);
  AddDisplayStringToProto(ClientSettingsProto::MAYBE_GIVE_UP, "maybe_give_up",
                          proto);
  AddDisplayStringToProto(ClientSettingsProto::SEND_FEEDBACK, "", proto);

  ClientSettings client_settings;
  client_settings.UpdateFromProto(proto);

  EXPECT_EQ(GetDisplayStringUTF8(ClientSettingsProto::GIVE_UP, client_settings),
            "give_up");
  EXPECT_EQ(
      GetDisplayStringUTF8(ClientSettingsProto::MAYBE_GIVE_UP, client_settings),
      "maybe_give_up");
  EXPECT_EQ(GetDisplayStringUTF8(ClientSettingsProto::UNDO, client_settings),
            "undo");
  // We should return empty string if set by the backend.
  EXPECT_EQ(
      GetDisplayStringUTF8(ClientSettingsProto::SEND_FEEDBACK, client_settings),
      "");
  // Display String not set in ClientSettings should return Chrome string.
  EXPECT_EQ(
      GetDisplayStringUTF8(ClientSettingsProto::DEFAULT_ERROR, client_settings),
      l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_DEFAULT_ERROR));
}

}  // namespace
}  // namespace autofill_assistant
