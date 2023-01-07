// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/display_strings_util.h"

#include "ui/base/l10n/l10n_util.h"

namespace autofill_assistant {
namespace {

int MapDisplayStringIdToChromeMessage(
    ClientSettingsProto::DisplayStringId display_string_id) {
  switch (display_string_id) {
    case ClientSettingsProto::GIVE_UP:
      return IDS_AUTOFILL_ASSISTANT_GIVE_UP;
    case ClientSettingsProto::MAYBE_GIVE_UP:
      return IDS_AUTOFILL_ASSISTANT_MAYBE_GIVE_UP;
    case ClientSettingsProto::DEFAULT_ERROR:
      return IDS_AUTOFILL_ASSISTANT_DEFAULT_ERROR;
    case ClientSettingsProto::PAYMENT_INFO_CONFIRM:
      return IDS_AUTOFILL_ASSISTANT_PAYMENT_INFO_CONFIRM;
    case ClientSettingsProto::CONTINUE_BUTTON:
      return IDS_AUTOFILL_ASSISTANT_CONTINUE_BUTTON;
    case ClientSettingsProto::STOPPED:
      return IDS_AUTOFILL_ASSISTANT_STOPPED;
    case ClientSettingsProto::SEND_FEEDBACK:
      return IDS_AUTOFILL_ASSISTANT_SEND_FEEDBACK;
    case ClientSettingsProto::CLOSE:
      return IDS_CLOSE;
    case ClientSettingsProto::SETTINGS:
      return IDS_SETTINGS_TITLE;

    case ClientSettingsProto::UNSPECIFIED:
    case ClientSettingsProto::UNDO:
      // This should not happen.
      return -1;
  }
}

}  // namespace

const std::string GetDisplayStringUTF8(
    ClientSettingsProto::DisplayStringId display_string_id,
    const ClientSettings& client_settings) {
  auto it = client_settings.display_strings.find(display_string_id);
  if (it != client_settings.display_strings.end()) {
    // Note that we return the string even if it is empty.
    return it->second;
  }
  if (display_string_id == ClientSettingsProto::UNDO) {
    // TODO(b/201396990) Provide fallback string for UNDO.
    // If this is empty, the java side will fallback to Chrome's IDS_UNDO.
    // IDS_UNDO is currently not available in native.
    if (client_settings.back_button_settings) {
      return client_settings.back_button_settings->undo_label();
    }
    return std::string();
  }
  if (display_string_id == ClientSettingsProto::UNSPECIFIED) {
    return std::string();
  }
  return l10n_util::GetStringUTF8(
      MapDisplayStringIdToChromeMessage(display_string_id));
}

}  // namespace autofill_assistant
