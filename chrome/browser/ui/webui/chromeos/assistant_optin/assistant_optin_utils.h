// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_ASSISTANT_OPTIN_ASSISTANT_OPTIN_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_ASSISTANT_OPTIN_ASSISTANT_OPTIN_UTILS_H_

#include <string>

#include "chromeos/services/assistant/public/proto/settings_ui.pb.h"

class PrefService;
class Profile;

namespace base {
class Value;
}  // namespace base

namespace chromeos {

// Type of Assistant opt-in flow status. This enum is used to back an UMA
// histogram and should be treated as append-only.
enum AssistantOptInFlowStatus {
  FLOW_STARTED = 0,
  ACTIVITY_CONTROL_SHOWN = 1,
  ACTIVITY_CONTROL_ACCEPTED = 2,
  ACTIVITY_CONTROL_SKIPPED = 3,
  THIRD_PARTY_SHOWN = 4,
  THIRD_PARTY_CONTINUED = 5,
  GET_MORE_SHOWN = 6,
  EMAIL_OPTED_IN = 7,
  EMAIL_OPTED_OUT = 8,
  GET_MORE_CONTINUED = 9,
  READY_SCREEN_SHOWN = 10,
  READY_SCREEN_CONTINUED = 11,
  VOICE_MATCH_SHOWN = 12,
  VOICE_MATCH_ENROLLMENT_DONE = 13,
  VOICE_MATCH_ENROLLMENT_SKIPPED = 14,
  VOICE_MATCH_ENROLLMENT_ERROR = 15,
  // Magic constant used by the histogram macros.
  kMaxValue = VOICE_MATCH_ENROLLMENT_ERROR
};

void RecordAssistantOptInStatus(AssistantOptInFlowStatus);

// Construct SettingsUiSelector for the ConsentFlow UI.
assistant::SettingsUiSelector GetSettingsUiSelector();

// Construct SettingsUiUpdate for user opt-in.
assistant::SettingsUiUpdate GetSettingsUiUpdate(
    const std::string& consent_token);

// Construct SettingsUiUpdate for email opt-in.
assistant::SettingsUiUpdate GetEmailOptInUpdate(bool opted_in);

using SettingZippyList = google::protobuf::RepeatedPtrField<
    assistant::ClassicActivityControlUiTexts::SettingZippy>;
// Helper method to create zippy data.
base::Value CreateZippyData(const SettingZippyList& zippy_list);

// Helper method to create disclosure data.
base::Value CreateDisclosureData(const SettingZippyList& disclosure_list);

// Helper method to create get more screen data.
base::Value CreateGetMoreData(bool email_optin_needed,
                              const assistant::EmailOptInUi& email_optin_ui,
                              PrefService* prefs);

// Get string constants for settings ui.
base::Value GetSettingsUiStrings(const assistant::SettingsUi& settings_ui,
                                 bool activity_control_needed);

void RecordActivityControlConsent(Profile* profile,
                                  std::string ui_audit_key,
                                  bool opted_in);

bool IsHotwordDspAvailable();

bool IsVoiceMatchEnforcedOff(const PrefService* prefs);

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_ASSISTANT_OPTIN_ASSISTANT_OPTIN_UTILS_H_
