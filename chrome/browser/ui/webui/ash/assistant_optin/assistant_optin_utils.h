// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_ASSISTANT_OPTIN_ASSISTANT_OPTIN_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_ASSISTANT_OPTIN_ASSISTANT_OPTIN_UTILS_H_

#include <string>

#include "base/values.h"
#include "chromeos/ash/services/assistant/public/proto/settings_ui.pb.h"
#include "components/sync/protocol/user_consent_types.pb.h"

class PrefService;
class Profile;

namespace ash {

// Type of Assistant opt-in flow status. This enum is used to back an UMA
// histogram and should be treated as append-only.
enum class AssistantOptInFlowStatus {
  kFlowStarted = 0,
  kActivityControlShown = 1,
  kActivityControlAccepted = 2,
  kActivityControlSkipped = 3,
  kThirdPartyShown = 4,
  kThirdPartyContinued = 5,
  kGetMoreShown = 6,
  kEmailOptedIn = 7,
  kEmailOptedOut = 8,
  kGetMoreContinued = 9,
  kReadyScreenShown = 10,
  kReadyScreenContinued = 11,
  kVoiceMatchShown = 12,
  kVoiceMatchEnrollmentDone = 13,
  kVoiceMatchEnrollmentSkipped = 14,
  kVoiceMatchEnrollmentError = 15,
  kRelatedInfoShown = 16,
  kRelatedInfoAccepted = 17,
  kRelatedInfoSkipped = 18,
  kActivityControlDaAccepted = 19,
  kActivityControlDaSkipped = 20,
  kActivityControlWaaAccepted = 21,
  kActivityControlWaaSkipped = 22,
  // Magic constant used by the histogram macros.
  kMaxValue = kActivityControlWaaSkipped
};

void RecordAssistantOptInStatus(AssistantOptInFlowStatus);
void RecordAssistantActivityControlOptInStatus(
    sync_pb::UserConsentTypes::AssistantActivityControlConsent::SettingType
        setting_type,
    bool opted_in);

// Construct SettingsUiSelector for the ConsentFlow UI.
assistant::SettingsUiSelector GetSettingsUiSelector();

// Construct SettingsUiUpdate for user opt-in.
assistant::SettingsUiUpdate GetSettingsUiUpdate(
    const std::string& consent_token);

using SettingZippyList = google::protobuf::RepeatedPtrField<
    assistant::ClassicActivityControlUiTexts::SettingZippy>;
using ActivityControlUi =
    assistant::ConsentFlowUi::ConsentUi::ActivityControlUi;
// Helper method to create zippy data.
base::Value::List CreateZippyData(const ActivityControlUi& activity_control_ui,
                                  bool is_minor_mode);

// Helper method to create disclosure data.
base::Value::List CreateDisclosureData(const SettingZippyList& disclosure_list);

// Get string constants for settings ui.
base::Value::Dict GetSettingsUiStrings(const assistant::SettingsUi& settings_ui,
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

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_ASSISTANT_OPTIN_ASSISTANT_OPTIN_UTILS_H_
