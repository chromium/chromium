// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/captions_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "media/base/media_switches.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN) || defined(OS_MAC)
#include "chrome/browser/accessibility/caption_settings_dialog.h"
#endif

namespace settings {

CaptionsHandler::CaptionsHandler(PrefService* prefs) : prefs_(prefs) {}

CaptionsHandler::~CaptionsHandler() {
  speech::SodaInstaller::GetInstance()->RemoveObserver(this);
}

void CaptionsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "openSystemCaptionsDialog",
      base::BindRepeating(&CaptionsHandler::HandleOpenSystemCaptionsDialog,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "liveCaptionSectionReady",
      base::BindRepeating(&CaptionsHandler::HandleLiveCaptionSectionReady,
                          base::Unretained(this)));
}

void CaptionsHandler::OnJavascriptAllowed() {
  speech::SodaInstaller::GetInstance()->AddObserver(this);
}

void CaptionsHandler::OnJavascriptDisallowed() {
  speech::SodaInstaller::GetInstance()->RemoveObserver(this);
}

void CaptionsHandler::HandleLiveCaptionSectionReady(
    const base::ListValue* args) {
  AllowJavascript();
}

void CaptionsHandler::HandleOpenSystemCaptionsDialog(
    const base::ListValue* args) {
#if defined(OS_WIN) || defined(OS_MAC)
  captions::CaptionSettingsDialog::ShowCaptionSettingsDialog();
#endif
}

void CaptionsHandler::OnSodaInstalled() {
  speech::SodaInstaller::GetInstance()->RemoveObserver(this);
  FireWebUIListener("soda-download-progress-changed",
                    base::Value(l10n_util::GetStringUTF16(
                        IDS_SETTINGS_CAPTIONS_LIVE_CAPTION_DOWNLOAD_COMPLETE)));
}

void CaptionsHandler::OnSodaError() {
  if (!base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage)) {
    prefs_->SetBoolean(prefs::kLiveCaptionEnabled, false);
  }

  FireWebUIListener("soda-download-progress-changed",
                    base::Value(l10n_util::GetStringUTF16(
                        IDS_SETTINGS_CAPTIONS_LIVE_CAPTION_DOWNLOAD_ERROR)));
}

void CaptionsHandler::OnSodaProgress(int progress) {
  FireWebUIListener(
      "soda-download-progress-changed",
      base::Value(l10n_util::GetStringFUTF16Int(
          IDS_SETTINGS_CAPTIONS_LIVE_CAPTION_DOWNLOAD_PROGRESS, progress)));
}

}  // namespace settings
