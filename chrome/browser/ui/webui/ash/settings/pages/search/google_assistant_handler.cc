// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/search/google_assistant_handler.h"

#include <utility>

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/public/cpp/assistant/assistant_setup.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/assistant_optin/assistant_optin_ui.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "content/public/browser/browser_context.h"
#include "ui/gfx/geometry/rect.h"

namespace ash::settings {

GoogleAssistantHandler::GoogleAssistantHandler() {
  CrasAudioHandler::Get()->AddAudioObserver(this);
}

GoogleAssistantHandler::~GoogleAssistantHandler() {
  CrasAudioHandler::Get()->RemoveAudioObserver(this);
}

void GoogleAssistantHandler::OnJavascriptAllowed() {
  if (pending_hotword_update_) {
    OnAudioNodesChanged();
  }
}

void GoogleAssistantHandler::OnJavascriptDisallowed() {}

void GoogleAssistantHandler::OnAudioNodesChanged() {
  if (!IsJavascriptAllowed()) {
    pending_hotword_update_ = true;
    return;
  }

  pending_hotword_update_ = false;
  FireWebUIListener("hotwordDeviceUpdated",
                    base::Value(CrasAudioHandler::Get()->HasHotwordDevice()));
}

void GoogleAssistantHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "showGoogleAssistantSettings",
      base::BindRepeating(
          &GoogleAssistantHandler::HandleShowGoogleAssistantSettings,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "retrainAssistantVoiceModel",
      base::BindRepeating(&GoogleAssistantHandler::HandleRetrainVoiceModel,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "syncVoiceModelStatus",
      base::BindRepeating(&GoogleAssistantHandler::HandleSyncVoiceModelStatus,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "initializeGoogleAssistantPage",
      base::BindRepeating(&GoogleAssistantHandler::HandleInitialized,
                          base::Unretained(this)));
}

void GoogleAssistantHandler::HandleShowGoogleAssistantSettings(
    const base::Value::List& args) {
  CHECK_EQ(0U, args.size());
  AssistantController::Get()->OpenAssistantSettings();
}

void GoogleAssistantHandler::HandleRetrainVoiceModel(
    const base::Value::List& args) {
  CHECK_EQ(0U, args.size());
  AssistantOptInDialog::Show(FlowType::kSpeakerIdRetrain, base::DoNothing());
}

void GoogleAssistantHandler::HandleSyncVoiceModelStatus(
    const base::Value::List& args) {
  CHECK_EQ(0U, args.size());

  auto* settings = assistant::AssistantSettings::Get();
  if (settings) {
    settings->SyncSpeakerIdEnrollmentStatus();
  }
}

void GoogleAssistantHandler::HandleInitialized(const base::Value::List& args) {
  CHECK_EQ(0U, args.size());
  AllowJavascript();
}

}  // namespace ash::settings
