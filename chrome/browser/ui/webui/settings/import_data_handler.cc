// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/import_data_handler.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
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
#include "ui/shell_dialogs/selected_file_info.h"

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

  // When the WebUI is unloading, we ignore all further updates from the host.
  // Because we're no longer listening to the `ImportEnded` callback, we must
  // also clear our pointer, as otherwise this can lead to a use-after-free
  // in the destructor. https://crbug.com/1302813.
  if (importer_host_) {
    importer_host_->set_observer(nullptr);
    importer_host_ = nullptr;
  }
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

void ImportDataHandler::HandleImportData(const base::Value::List& args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const auto& list = args;
  CHECK_GE(list.size(), 2u);

  int browser_index = list[0].GetInt();

  const base::Value& types = list[1];
  CHECK(types.is_dict());

  if (!importer_list_loaded_ || browser_index < 0 ||
      browser_index >= static_cast<int>(importer_list_->count())) {
    // Prevent out-of-bounds access.
    return;
  }

  const base::Value::Dict& type_dict = types.GetDict();
  uint16_t selected_items = importer::NONE;
  if (*type_dict.FindBool(prefs::kImportDialogAutofillFormData)) {
    selected_items |= importer::AUTOFILL_FORM_DATA;
  }
  if (*type_dict.FindBool(prefs::kImportDialogBookmarks)) {
    selected_items |= importer::FAVORITES;
  }
  if (*type_dict.FindBool(prefs::kImportDialogHistory)) {
    selected_items |= importer::HISTORY;
  }
  if (*type_dict.FindBool(prefs::kImportDialogSavedPasswords)) {
    selected_items |= importer::PASSWORDS;
  }
  if (*type_dict.FindBool(prefs::kImportDialogSearchEngine)) {
    selected_items |= importer::SEARCH_ENGINES;
  }

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
    const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  const std::string& callback_id = args[0].GetString();

  importer_list_ = std::make_unique<ImporterList>();
  importer_list_->DetectSourceProfiles(
      g_browser_process->GetApplicationLocale(),
      true,  // include_interactive_profiles
      base::BindOnce(&ImportDataHandler::SendBrowserProfileData,
                     base::Unretained(this), callback_id));
}

void ImportDataHandler::HandleImportFromBookmarksFile(
    const base::Value::List& args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (select_file_dialog_)
    return;

  DCHECK(args.empty());
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this,
      std::make_unique<ChromeSelectFilePolicy>(web_ui()->GetWebContents()));

  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(1);
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("html"));

  Browser* browser = chrome::FindBrowserWithTab(web_ui()->GetWebContents());

  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE, std::u16string(),
      base::FilePath(), &file_type_info, 0, base::FilePath::StringType(),
      browser->window()->GetNativeWindow(), nullptr);
}

void ImportDataHandler::SendBrowserProfileData(const std::string& callback_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  importer_list_loaded_ = true;

  base::Value::List browser_profiles;
  for (size_t i = 0; i < importer_list_->count(); ++i) {
    const importer::SourceProfile& source_profile =
        importer_list_->GetSourceProfileAt(i);
    uint16_t browser_services = source_profile.services_supported;

    base::Value::Dict browser_profile;
    browser_profile.Set("name", source_profile.importer_name);
    browser_profile.Set("index", static_cast<int>(i));
    browser_profile.Set("profileName", source_profile.profile);
    browser_profile.Set("history", (browser_services & importer::HISTORY) != 0);
    browser_profile.Set("favorites",
                        (browser_services & importer::FAVORITES) != 0);
    browser_profile.Set("passwords",
                        (browser_services & importer::PASSWORDS) != 0);
    browser_profile.Set("search",
                        (browser_services & importer::SEARCH_ENGINES) != 0);
    browser_profile.Set("autofillFormData",
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

void ImportDataHandler::FileSelected(const ui::SelectedFileInfo& file,
                                     int /*index*/) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  select_file_dialog_ = nullptr;

  importer::SourceProfile source_profile;
  source_profile.importer_type = importer::TYPE_BOOKMARKS_FILE;
  source_profile.source_path = file.path();

  StartImport(source_profile, importer::FAVORITES);
}

void ImportDataHandler::FileSelectionCanceled() {
  select_file_dialog_ = nullptr;
}

}  // namespace settings
