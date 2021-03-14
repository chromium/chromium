// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/import_data_handler.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/importer/external_process_importer_host.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/browser/importer/importer_uma.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"

using content::BrowserThread;

namespace settings {

namespace {
const char kImportStatusInProgress[] = "inProgress";
const char kImportStatusSucceeded[] = "succeeded";
const char kImportStatusFailed[] = "failed";
}  // namespace

ImportDataHandler::ImportDataHandler() : importer_host_(nullptr) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

ImportDataHandler::~ImportDataHandler() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (importer_host_)
    importer_host_->set_observer(nullptr);

  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();
}

void ImportDataHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  web_ui()->RegisterMessageCallback(
      "initializeImportDialog",
      base::BindRepeating(&ImportDataHandler::HandleInitializeImportDialog,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "importData", base::BindRepeating(&ImportDataHandler::HandleImportData,
                                        base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "importFromBookmarksFile",
      base::BindRepeating(&ImportDataHandler::HandleImportFromBookmarksFile,
                          base::Unretained(this)));
}

void ImportDataHandler::OnJavascriptDisallowed() {
  // Cancels outstanding profile list detections.
  importer_list_.reset();

  // Stops listening to updates from any ongoing imports.
  if (importer_host_)
    importer_host_->set_observer(nullptr);
}

void ImportDataHandler::StartImport(
    const importer::SourceProfile& source_profile,
    uint16_t imported_items) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!imported_items)
    return;

  // If another import is already ongoing, let it finish silently.
  if (importer_host_)
    importer_host_->set_observer(nullptr);

  FireWebUIListener("import-data-status-changed",
                    base::Value(kImportStatusInProgress));
  import_did_succeed_ = false;

  importer_host_ = new ExternalProcessImporterHost();
  importer_host_->set_observer(this);
  Profile* profile = Profile::FromWebUI(web_ui());
  importer_host_->StartImportSettings(source_profile, profile, imported_items,
                                      new ProfileWriter(profile));

  importer::LogImporterUseToMetrics("ImportDataHandler",
                                    source_profile.importer_type);
}

void ImportDataHandler::HandleImportData(const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  int browser_index;
  CHECK(args->GetInteger(0, &browser_index));

  const base::DictionaryValue* types = nullptr;
  CHECK(args->GetDictionary(1, &types));

  if (!importer_list_loaded_ || browser_index < 0 ||
      browser_index >= static_cast<int>(importer_list_->count())) {
    // Prevent out-of-bounds access.
    return;
  }

  uint16_t selected_items = importer::NONE;
  if (*types->FindBoolKey(prefs::kImportDialogAutofillFormData))
    selected_items |= importer::AUTOFILL_FORM_DATA;
  if (*types->FindBoolKey(prefs::kImportDialogBookmarks))
    selected_items |= importer::FAVORITES;
  if (*types->FindBoolKey(prefs::kImportDialogHistory))
    selected_items |= importer::HISTORY;
  if (*types->FindBoolKey(prefs::kImportDialogSavedPasswords))
    selected_items |= importer::PASSWORDS;
  if (*types->FindBoolKey(prefs::kImportDialogSearchEngine))
    selected_items |= importer::SEARCH_ENGINES;

  const importer::SourceProfile& source_profile =
      importer_list_->GetSourceProfileAt(browser_index);
  uint16_t supported_items = source_profile.services_supported;

  uint16_t imported_items = (selected_items & supported_items);
  if (imported_items) {
    StartImport(source_profile, imported_items);
  } else {
    LOG(WARNING) << "There were no settings to import from '"
                 << source_profile.importer_name << "'.";
  }
}

void ImportDataHandler::HandleInitializeImportDialog(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1U, args->GetSize());
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  importer_list_ = std::make_unique<ImporterList>();
  importer_list_->DetectSourceProfiles(
      g_browser_process->GetApplicationLocale(),
      true,  // include_interactive_profiles
      base::BindOnce(&ImportDataHandler::SendBrowserProfileData,
                     base::Unretained(this), callback_id));
}

void ImportDataHandler::HandleImportFromBookmarksFile(
    const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK(args && args->empty());
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this,
      std::make_unique<ChromeSelectFilePolicy>(web_ui()->GetWebContents()));

  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(1);
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("html"));

  Browser* browser =
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents());

  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE, std::u16string(),
      base::FilePath(), &file_type_info, 0, base::FilePath::StringType(),
      browser->window()->GetNativeWindow(), nullptr);
}

void ImportDataHandler::SendBrowserProfileData(const std::string& callback_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  importer_list_loaded_ = true;

  base::ListValue browser_profiles;
  for (size_t i = 0; i < importer_list_->count(); ++i) {
    const importer::SourceProfile& source_profile =
        importer_list_->GetSourceProfileAt(i);
    uint16_t browser_services = source_profile.services_supported;

    std::unique_ptr<base::DictionaryValue> browser_profile(
        new base::DictionaryValue());
    browser_profile->SetString("name", source_profile.importer_name);
    browser_profile->SetInteger("index", i);
    browser_profile->SetString("profileName", source_profile.profile);
    browser_profile->SetBoolean("history",
                                (browser_services & importer::HISTORY) != 0);
    browser_profile->SetBoolean("favorites",
                                (browser_services & importer::FAVORITES) != 0);
    browser_profile->SetBoolean("passwords",
                                (browser_services & importer::PASSWORDS) != 0);
    browser_profile->SetBoolean(
        "search", (browser_services & importer::SEARCH_ENGINES) != 0);
    browser_profile->SetBoolean(
        "autofillFormData",
        (browser_services & importer::AUTOFILL_FORM_DATA) != 0);

    browser_profiles.Append(std::move(browser_profile));
  }

  ResolveJavascriptCallback(base::Value(callback_id), browser_profiles);
}

void ImportDataHandler::ImportStarted() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void ImportDataHandler::ImportItemStarted(importer::ImportItem item) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(csilv): show progress detail in the web view.
}

void ImportDataHandler::ImportItemEnded(importer::ImportItem item) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(csilv): show progress detail in the web view.
  import_did_succeed_ = true;
}

void ImportDataHandler::ImportEnded() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  importer_host_->set_observer(nullptr);
  importer_host_ = nullptr;

  FireWebUIListener("import-data-status-changed",
                    base::Value(import_did_succeed_ ? kImportStatusSucceeded
                                                    : kImportStatusFailed));
}

void ImportDataHandler::FileSelected(const base::FilePath& path,
                                     int /*index*/,
                                     void* /*params*/) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  importer::SourceProfile source_profile;
  source_profile.importer_type = importer::TYPE_BOOKMARKS_FILE;
  source_profile.source_path = path;

  StartImport(source_profile, importer::FAVORITES);
}

}  // namespace settings
