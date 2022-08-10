// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/downloads_handler.h"

#include "base/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/file_system/service_settings.h"
#include "chrome/browser/enterprise/connectors/file_system/signin_experience.h"
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/file_manager/path_util.h"
#endif

using base::UserMetricsAction;
namespace ec = enterprise_connectors;

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

  web_ui()->RegisterMessageCallback(
      "setDownloadsConnectionAccountLink",
      base::BindRepeating(
          &DownloadsHandler::HandleSetDownloadsConnectionAccountLink,
          base::Unretained(this)));
}

void DownloadsHandler::OnJavascriptAllowed() {
  pref_registrar_.Init(profile_->GetPrefs());
  pref_registrar_.Add(
      prefs::kDownloadExtensionsToOpen,
      base::BindRepeating(&DownloadsHandler::SendAutoOpenDownloadsToJavascript,
                          base::Unretained(this)));
  pref_registrar_.Add(
      enterprise_connectors::kSendDownloadToCloudPref,
      base::BindRepeating(
          &DownloadsHandler::SendDownloadsConnectionPolicyToJavascript,
          base::Unretained(this)));
}

void DownloadsHandler::OnJavascriptDisallowed() {
  pref_registrar_.RemoveAll();
}

void DownloadsHandler::HandleInitialize(const base::Value::List& args) {
  AllowJavascript();
  SendDownloadsConnectionPolicyToJavascript();
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
      web_ui()->GetWebContents()->GetTopLevelNativeWindow(), NULL);
}

void DownloadsHandler::FileSelected(const base::FilePath& path,
                                    int index,
                                    void* params) {
  select_folder_dialog_ = nullptr;

  base::RecordAction(UserMetricsAction("Options_SetDownloadDirectory"));
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetFilePath(prefs::kDownloadDefaultDirectory, path);
  pref_service->SetFilePath(prefs::kSaveFileDefaultDirectory, path);
}

void DownloadsHandler::FileSelectionCanceled(void* params) {
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

using enterprise_connectors::FileSystemSigninDialogDelegate;

bool DownloadsHandler::IsDownloadsConnectionPolicyEnabled() const {
  return ec::GetFileSystemSettings(profile_).has_value();
}

void DownloadsHandler::SendDownloadsConnectionPolicyToJavascript() {
  bool routing_enabled = IsDownloadsConnectionPolicyEnabled();
  if (routing_enabled) {
    std::vector<std::string> connection_prefs =
        ec::GetFileSystemConnectorPrefsForSettingsPage(profile_);
    for (const std::string& pref : connection_prefs) {
      if (pref_registrar_.IsObserved(pref))
        continue;
      pref_registrar_.Add(
          pref, base::BindRepeating(
                    &DownloadsHandler::SendDownloadsConnectionInfoToJavascript,
                    base::Unretained(this)));
    }
    SendDownloadsConnectionInfoToJavascript();
  }

  FireWebUIListener("downloads-connection-policy-changed",
                    base::Value(routing_enabled));
}

void DownloadsHandler::HandleSetDownloadsConnectionAccountLink(
    const base::Value::List& args) {
  DCHECK(IsDownloadsConnectionPolicyEnabled());
  CHECK_EQ(1U, args.size());
  bool enable_link = args[0].GetBool();
  ec::SetFileSystemConnectorAccountLinkForSettingsPage(
      enable_link, profile_,
      base::BindOnce(&DownloadsHandler::OnDownloadsConnectionAccountLinkSet,
                     weak_factory_.GetWeakPtr()));
}

void DownloadsHandler::OnDownloadsConnectionAccountLinkSet(bool success) {
  if (!success) {
    DLOG(ERROR) << "Failed to set downloads connection account link";
  }
  SendDownloadsConnectionInfoToJavascript();
}

void DownloadsHandler::SendDownloadsConnectionInfoToJavascript() {
  absl::optional<ec::FileSystemSettings> settings =
      ec::GetFileSystemSettings(profile_);

  absl::optional<ec::AccountInfo> info;
  bool got_linked_account =
      settings.has_value() && (info = GetFileSystemConnectorLinkedAccountInfo(
                                   settings.value(), profile_->GetPrefs()))
                                  .has_value();
  // Dict to match the fields used in downloads_page.html.
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetBoolKey("linked", got_linked_account);
  if (got_linked_account) {
    base::Value account(base::Value::Type::DICTIONARY);
    account.SetStringKey("name", info->account_name);
    account.SetStringKey("login", info->account_login);
    dict.SetKey("account", std::move(account));
    base::Value folder(base::Value::Type::DICTIONARY);
    folder.SetStringKey("name", info->folder_name);
    folder.SetStringKey("link", info->folder_link);
    dict.SetKey("folder", std::move(folder));
  }
  FireWebUIListener("downloads-connection-link-changed", dict);
}

}  // namespace settings
