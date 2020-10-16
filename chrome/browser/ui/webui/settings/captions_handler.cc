// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/captions_handler.h"

#include "base/bind.h"
#include "base/check_op.h"
#include "base/numerics/ranges.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/soda_component_installer.h"
#include "chrome/browser/component_updater/soda_en_us_component_installer.h"
#include "chrome/browser/component_updater/soda_ja_jp_component_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/crx_update_item.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN) || defined(OS_MAC)
#include "chrome/browser/accessibility/caption_settings_dialog.h"
#endif

namespace {

int GetDownloadProgress(std::map<std::string, update_client::CrxUpdateItem>
                            downloading_components) {
  int total_bytes = 0;
  int downloaded_bytes = 0;

  for (auto component : downloading_components) {
    if (component.second.downloaded_bytes >= 0 &&
        component.second.total_bytes > 0) {
      downloaded_bytes += component.second.downloaded_bytes;
      total_bytes += component.second.total_bytes;
    }
  }

  if (total_bytes == 0)
    return -1;

  DCHECK_LE(downloaded_bytes, total_bytes);
  return 100 *
         base::ClampToRange(double{downloaded_bytes} / total_bytes, 0.0, 1.0);
}

}  // namespace

namespace settings {

CaptionsHandler::CaptionsHandler(PrefService* prefs) : prefs_(prefs) {}

CaptionsHandler::~CaptionsHandler() = default;

void CaptionsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "openSystemCaptionsDialog",
      base::BindRepeating(&CaptionsHandler::HandleOpenSystemCaptionsDialog,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "captionsSubpageReady",
      base::BindRepeating(&CaptionsHandler::HandleCaptionsSubpageReady,
                          base::Unretained(this)));
}

void CaptionsHandler::OnJavascriptAllowed() {
  component_updater_observer_.Add(g_browser_process->component_updater());
}

void CaptionsHandler::OnJavascriptDisallowed() {
  component_updater_observer_.RemoveAll();
}

void CaptionsHandler::HandleCaptionsSubpageReady(const base::ListValue* args) {
  AllowJavascript();
}

void CaptionsHandler::HandleOpenSystemCaptionsDialog(
    const base::ListValue* args) {
#if defined(OS_WIN) || defined(OS_MAC)
  captions::CaptionSettingsDialog::ShowCaptionSettingsDialog();
#endif
}

void CaptionsHandler::OnEvent(Events event, const std::string& id) {
  if (id != component_updater::SODAComponentInstallerPolicy::GetExtensionId() &&
      id != component_updater::SodaEnUsComponentInstallerPolicy::
                GetExtensionId() &&
      id !=
          component_updater::SodaJaJpComponentInstallerPolicy::GetExtensionId())
    return;

  switch (event) {
    case Events::COMPONENT_UPDATE_FOUND:
    case Events::COMPONENT_UPDATE_READY:
    case Events::COMPONENT_WAIT:
    case Events::COMPONENT_UPDATE_DOWNLOADING:
    case Events::COMPONENT_UPDATE_UPDATING: {
      update_client::CrxUpdateItem item;
      g_browser_process->component_updater()->GetComponentDetails(id, &item);
      downloading_components_[id] = item;
      const int progress = GetDownloadProgress(downloading_components_);
      // When GetDownloadProgress returns -1, do nothing. It returns -1 when the
      // downloaded or total bytes is unknown.
      if (progress != -1) {
        FireWebUIListener(
            "enable-live-caption-subtitle-changed",
            base::Value(l10n_util::GetStringFUTF16Int(
                IDS_SETTINGS_CAPTIONS_LIVE_CAPTION_DOWNLOAD_PROGRESS,
                progress)));
      }
    } break;
    case Events::COMPONENT_UPDATED:
    case Events::COMPONENT_NOT_UPDATED:
      FireWebUIListener(
          "enable-live-caption-subtitle-changed",
          base::Value(l10n_util::GetStringUTF16(
              IDS_SETTINGS_CAPTIONS_LIVE_CAPTION_DOWNLOAD_COMPLETE)));
      break;
    case Events::COMPONENT_UPDATE_ERROR:
      prefs_->SetBoolean(prefs::kLiveCaptionEnabled, false);
      FireWebUIListener(
          "enable-live-caption-subtitle-changed",
          base::Value(l10n_util::GetStringUTF16(
              IDS_SETTINGS_CAPTIONS_LIVE_CAPTION_DOWNLOAD_ERROR)));
      break;
    case Events::COMPONENT_CHECKING_FOR_UPDATES:
      // Do nothing.
      break;
  }
}

}  // namespace settings
