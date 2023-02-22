// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/privacy_hub_handler.h"

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/ash/privacy_hub/privacy_hub_hats_trigger.h"
#include "chrome/browser/ash/privacy_hub/privacy_hub_util.h"
#include "chrome/common/chrome_features.h"

namespace ash::settings {

PrivacyHubHandler::PrivacyHubHandler() = default;

PrivacyHubHandler::~PrivacyHubHandler() {
  TriggerHatsIfPageWasOpened();
  privacy_hub_util::SetFrontend(nullptr);
}

void PrivacyHubHandler::RegisterMessages() {
  if (ash::features::IsCrosPrivacyHubEnabled()) {
    privacy_hub_util::SetFrontend(this);
    web_ui()->RegisterMessageCallback(
        "getInitialMicrophoneHardwareToggleState",
        base::BindRepeating(
            &PrivacyHubHandler::HandleInitialMicrophoneSwitchState,
            base::Unretained(this)));
  }

  if (base::FeatureList::IsEnabled(
          ::features::kHappinessTrackingPrivacyHubBaseline)) {
    web_ui()->RegisterMessageCallback(
        "osPrivacyPageWasOpened",
        base::BindRepeating(&PrivacyHubHandler::HandlePrivacyPageOpened,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "leftOsPrivacyPage",
        base::BindRepeating(&PrivacyHubHandler::HandlePrivacyPageClosed,
                            base::Unretained(this)));
  }
}

void PrivacyHubHandler::NotifyJS(const std::string& event_name,
                                 const base::Value& value) {
  if (IsJavascriptAllowed()) {
    FireWebUIListener(event_name, value);
  } else {
    DVLOG(1) << "JS disabled. Skip \"" << event_name
             << "\" event until enabled.";
  }
}

void PrivacyHubHandler::HandleInitialMicrophoneSwitchState(
    const base::Value::List& args) {
  DCHECK(ash::features::IsCrosPrivacyHubEnabled());
  AllowJavascript();

  DCHECK_GE(1U, args.size()) << ": Did not expect arguments";
  DCHECK_EQ(1U, args.size()) << ": Callback ID is required";
  const auto& callback_id = args[0];
  const base::Value value =
      base::Value(privacy_hub_util::MicrophoneSwitchState());

  ResolveJavascriptCallback(callback_id, value);
}

void PrivacyHubHandler::MicrophoneHardwareToggleChanged(bool muted) {
  DCHECK(ash::features::IsCrosPrivacyHubEnabled());
  NotifyJS("microphone-hardware-toggle-changed", base::Value(muted));
}

void PrivacyHubHandler::HandlePrivacyPageOpened(const base::Value::List& args) {
  DCHECK(args.empty());
  DCHECK(base::FeatureList::IsEnabled(
      ::features::kHappinessTrackingPrivacyHubBaseline));

  AllowJavascript();

  privacy_page_was_opened_ = true;
}

void PrivacyHubHandler::HandlePrivacyPageClosed(const base::Value::List& args) {
  DCHECK(args.empty());
  DCHECK(base::FeatureList::IsEnabled(
      ::features::kHappinessTrackingPrivacyHubBaseline));

  AllowJavascript();

  TriggerHatsIfPageWasOpened();
}

void PrivacyHubHandler::TriggerHatsIfPageWasOpened() {
  if (privacy_page_was_opened_) {
    PrivacyHubHatsTrigger::Get().ShowSurveyAfterDelayElapsed();
  }
}

}  // namespace ash::settings
