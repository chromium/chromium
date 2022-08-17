// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_ASSISTANT_OPTIN_ASSISTANT_OPTIN_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_ASSISTANT_OPTIN_ASSISTANT_OPTIN_UTILS_H_

#include <string>

#include "base/values.h"
#include "chromeos/ash/services/assistant/public/proto/settings_ui.pb.h"
#include "components/sync/protocol/user_consent_types.pb.h"

class PrefService;
class Profile;

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
  RELATED_INFO_SHOWN = 16,
  RELATED_INFO_ACCEPTED = 17,
  RELATED_INFO_SKIPPED = 18,
  ACTIVITY_CONTROL_DA_ACCEPTED = 19,
  ACTIVITY_CONTROL_DA_SKIPPED = 20,
  ACTIVITY_CONTROL_WAA_ACCEPTED = 21,
  ACTIVITY_CONTROL_WAA_SKIPPED = 22,
  // Magic constant used by the histogram macros.
  kMaxValue = ACTIVITY_CONTROL_WAA_SKIPPED
};

void RecordAssistantOptInStatus(AssistantOptInFlowStatus);
void RecordAssistantActivityControlOptInStatus(
    sync_pb::UserConsentTypes::AssistantActivityControlConsent::SettingType
        setting_type,
    bool opted_in);

// Construct SettingsUiSelector for the ConsentFlow UI.
ash::assistant::SettingsUiSelector GetSettingsUiSelector();

// Construct SettingsUiUpdate for user opt-in.
ash::assistant::SettingsUiUpdate GetSettingsUiUpdate(
    const std::string& consent_token);

using SettingZippyList = google::protobuf::RepeatedPtrField<
    ash::assistant::ClassicActivityControlUiTexts::SettingZippy>;
using ActivityControlUi =
    ash::assistant::ConsentFlowUi::ConsentUi::ActivityControlUi;
// Helper method to create zippy data.
base::Value::List CreateZippyData(const ActivityControlUi& activity_control_ui,
                                  bool is_minor_mode);

// Helper method to create disclosure data.
base::Value::List CreateDisclosureData(const SettingZippyList& disclosure_list);

// Get string constants for settings ui.
base::Value::Dict GetSettingsUiStrings(
    const ash::assistant::SettingsUi& settings_ui,
    bool activity_control_needed,
    bool equal_weight_buttons);

void RecordActivityControlConsent(
    Profile* profile,
    std::string ui_audit_key,
    bool opted_in,
    sync_pb::UserConsentTypes::AssistantActivityControlConsent::SettingType
        setting_type);

bool IsHotwordDspAvailable();

bool IsVoiceMatchEnforcedOff(const PrefService* prefs,
                             bool is_oobe_in_progress);

sync_pb::UserConsentTypes::AssistantActivityControlConsent::SettingType
GetActivityControlConsentSettingType(const SettingZippyList& setting_zippys);

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_ASSISTANT_OPTIN_ASSISTANT_OPTIN_UTILS_H_
