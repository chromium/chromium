// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/downloads_handler.h"

#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/selected_file_info.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/file_manager/path_util.h"
#endif

using base::UserMetricsAction;

namespace settings {

DownloadsHandler::DownloadsHandler(Profile* profile) : profile_(profile) {}

DownloadsHandler::~DownloadsHandler() {
  // There may be pending file dialogs, we need to tell them that we've gone
  // away so they don't try and call back to us.
  if (select_folder_dialog_)
    select_folder_dialog_->ListenerDestroyed();
}

void DownloadsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "initializeDownloads",
      base::BindRepeating(&DownloadsHandler::HandleInitialize,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "resetAutoOpenFileTypes",
      base::BindRepeating(&DownloadsHandler::HandleResetAutoOpenFileTypes,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "selectDownloadLocation",
      base::BindRepeating(&DownloadsHandler::HandleSelectDownloadLocation,
                          base::Unretained(this)));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  web_ui()->RegisterMessageCallback(
      "getDownloadLocationText",
      base::BindRepeating(&DownloadsHandler::HandleGetDownloadLocationText,
                          base::Unretained(this)));
#endif
}

void DownloadsHandler::OnJavascriptAllowed() {
  pref_registrar_.Init(profile_->GetPrefs());
  pref_registrar_.Add(
      prefs::kDownloadExtensionsToOpen,
      base::BindRepeating(&DownloadsHandler::SendAutoOpenDownloadsToJavascript,
                          base::Unretained(this)));
}

void DownloadsHandler::OnJavascriptDisallowed() {
  pref_registrar_.RemoveAll();
}

void DownloadsHandler::HandleInitialize(const base::Value::List& args) {
  AllowJavascript();
  SendAutoOpenDownloadsToJavascript();
}

void DownloadsHandler::SendAutoOpenDownloadsToJavascript() {
  content::DownloadManager* manager = profile_->GetDownloadManager();
  bool auto_open_downloads =
      DownloadPrefs::FromDownloadManager(manager)->IsAutoOpenByUserUsed();
  FireWebUIListener("auto-open-downloads-changed",
                    base::Value(auto_open_downloads));
}

void DownloadsHandler::HandleResetAutoOpenFileTypes(
    const base::Value::List& args) {
  base::RecordAction(UserMetricsAction("Options_ResetAutoOpenFiles"));
  content::DownloadManager* manager = profile_->GetDownloadManager();
  DownloadPrefs::FromDownloadManager(manager)->ResetAutoOpenByUser();
}

void DownloadsHandler::HandleSelectDownloadLocation(
    const base::Value::List& args) {
  // Early return if the select folder dialog is already active.
  if (select_folder_dialog_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  select_folder_dialog_ = ui::SelectFileDialog::Create(
      this,
      std::make_unique<ChromeSelectFilePolicy>(web_ui()->GetWebContents()));
  ui::SelectFileDialog::FileTypeInfo info;
  info.allowed_paths = ui::SelectFileDialog::FileTypeInfo::NATIVE_PATH;
  select_folder_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_FOLDER,
      l10n_util::GetStringUTF16(IDS_SETTINGS_DOWNLOAD_LOCATION),
      pref_service->GetFilePath(prefs::kDownloadDefaultDirectory), &info, 0,
      base::FilePath::StringType(),
      web_ui()->GetWebContents()->GetTopLevelNativeWindow(), nullptr);
}

void DownloadsHandler::FileSelected(const ui::SelectedFileInfo& file,
                                    int index) {
  select_folder_dialog_ = nullptr;

  base::RecordAction(UserMetricsAction("Options_SetDownloadDirectory"));
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetFilePath(prefs::kDownloadDefaultDirectory, file.path());
  pref_service->SetFilePath(prefs::kSaveFileDefaultDirectory, file.path());
}

void DownloadsHandler::FileSelectionCanceled() {
  select_folder_dialog_ = nullptr;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void DownloadsHandler::HandleGetDownloadLocationText(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(2U, args.size());
  const std::string& callback_id = args[0].GetString();
  const std::string& path = args[1].GetString();

  ResolveJavascriptCallback(
      base::Value(callback_id),
      base::Value(
          file_manager::util::GetPathDisplayTextForSettings(profile_, path)));
}
#endif

}  // namespace settings
