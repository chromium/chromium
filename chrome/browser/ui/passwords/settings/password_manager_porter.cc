// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/settings/password_manager_porter.h"

#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/export/password_manager_exporter.h"
#include "components/password_manager/core/browser/import/csv_password_sequence.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "net/base/filename_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#endif

namespace {

// The following are not used on Android due to the |SelectFileDialog| being
// unused.
#if !defined(OS_ANDROID)
const base::FilePath::CharType kFileExtension[] = FILE_PATH_LITERAL("csv");

// Returns the file extensions corresponding to supported formats.
// Inner vector indicates equivalent extensions. For example:
//   { { "html", "htm" }, { "csv" } }
std::vector<std::vector<base::FilePath::StringType>>
GetSupportedFileExtensions() {
  return std::vector<std::vector<base::FilePath::StringType>>(
      1, std::vector<base::FilePath::StringType>(1, kFileExtension));
}

// The default directory and filename when importing and exporting passwords.
base::FilePath GetDefaultFilepathForPasswordFile(
    const base::FilePath::StringType& default_extension) {
  base::FilePath default_path;
  base::PathService::Get(chrome::DIR_USER_DOCUMENTS, &default_path);
#if defined(OS_WIN)
  std::wstring file_name = base::UTF8ToWide(
      l10n_util::GetStringUTF8(IDS_PASSWORD_MANAGER_DEFAULT_EXPORT_FILENAME));
#else
  std::string file_name =
      l10n_util::GetStringUTF8(IDS_PASSWORD_MANAGER_DEFAULT_EXPORT_FILENAME);
#endif
  return default_path.Append(file_name).AddExtension(default_extension);
}
#endif

// A helper class for reading the passwords that have been imported.
class PasswordImportConsumer {
 public:
  explicit PasswordImportConsumer(Profile* profile);

  void ConsumePassword(password_manager::PasswordImporter::Result result,
                       password_manager::CSVPasswordSequence seq);

 private:
  Profile* profile_;
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(PasswordImportConsumer);
};

PasswordImportConsumer::PasswordImportConsumer(Profile* profile)
    : profile_(profile) {}

void PasswordImportConsumer::ConsumePassword(
    password_manager::PasswordImporter::Result result,
    password_manager::CSVPasswordSequence seq) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.ImportPasswordFromCSVResult", result,
      password_manager::PasswordImporter::NUM_IMPORT_RESULTS);

  if (result != password_manager::PasswordImporter::SUCCESS)
    return;

  scoped_refptr<password_manager::PasswordStore> store(
      PasswordStoreFactory::GetForProfile(profile_,
                                          ServiceAccessType::EXPLICIT_ACCESS));
  for (const auto& pwd : seq) {
    if (store)
      store->AddLogin(pwd.ParseValid());
  }
  // TODO(crbug.com/1025510): Should the length of |seq| be reported if
  // |store| is null?
  UMA_HISTOGRAM_COUNTS_1M("PasswordManager.ImportedPasswordsPerUserInCSV",
                          std::distance(seq.begin(), seq.end()));
}

}  // namespace

PasswordManagerPorter::PasswordManagerPorter(
    password_manager::CredentialProviderInterface*
        credential_provider_interface,
    ProgressCallback on_export_progress_callback)
    : credential_provider_interface_(credential_provider_interface),
      on_export_progress_callback_(on_export_progress_callback) {}

PasswordManagerPorter::~PasswordManagerPorter() = default;

bool PasswordManagerPorter::Store() {
  // In unittests a null WebContents means: "Abort creating the file Selector."
  if (!web_contents_)
    return true;

  if (exporter_ && exporter_->GetProgressStatus() ==
                       password_manager::ExportProgressStatus::IN_PROGRESS) {
    return false;
  }

  // Set a new exporter for this request.
  exporter_ =
      exporter_for_testing_
          ? std::move(exporter_for_testing_)
          : std::make_unique<password_manager::PasswordManagerExporter>(
                credential_provider_interface_, on_export_progress_callback_);

  // Start serialising while the user selects a file.
  exporter_->PreparePasswordsForExport();
  PresentFileSelector(web_contents_,
                      PasswordManagerPorter::Type::PASSWORD_EXPORT);

  return true;
}

void PasswordManagerPorter::CancelStore() {
  if (exporter_)
    exporter_->Cancel();
}

password_manager::ExportProgressStatus
PasswordManagerPorter::GetExportProgressStatus() {
  return exporter_ ? exporter_->GetProgressStatus()
                   : password_manager::ExportProgressStatus::NOT_STARTED;
}

void PasswordManagerPorter::SetExporterForTesting(
    std::unique_ptr<password_manager::PasswordManagerExporter> exporter) {
  exporter_for_testing_ = std::move(exporter);
}

void PasswordManagerPorter::Load() {
  DCHECK(web_contents_);
  PresentFileSelector(web_contents_,
                      PasswordManagerPorter::Type::PASSWORD_IMPORT);
}

void PasswordManagerPorter::ImportPasswordsFromPathForTesting(
    const base::FilePath& path,
    Profile* profile) {
  base::AutoReset<Profile*> reset(&profile_, profile);
  ImportPasswordsFromPath(path);
}

void PasswordManagerPorter::PresentFileSelector(
    content::WebContents* web_contents,
    Type type) {
// This method should never be called on Android (as there is no file selector),
// and the relevant IDS constants are not present for Android.
#if !defined(OS_ANDROID)
  DCHECK(web_contents);
  profile_ = Profile::FromBrowserContext(web_contents->GetBrowserContext());

  // Get the default file extension for password files.
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions = GetSupportedFileExtensions();
  DCHECK(!file_type_info.extensions.empty());
  DCHECK(!file_type_info.extensions[0].empty());
  file_type_info.include_all_files = true;

  // Present the file selector dialogue.
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents));

  ui::SelectFileDialog::Type file_selector_mode =
      ui::SelectFileDialog::SELECT_NONE;
  unsigned title = 0;
  switch (type) {
    case PASSWORD_IMPORT:
      file_selector_mode = ui::SelectFileDialog::SELECT_OPEN_FILE;
      title = IDS_PASSWORD_MANAGER_IMPORT_DIALOG_TITLE;
      break;
    case PASSWORD_EXPORT:
      file_selector_mode = ui::SelectFileDialog::SELECT_SAVEAS_FILE;
      title = IDS_PASSWORD_MANAGER_EXPORT_DIALOG_TITLE;
      break;
  }
  // Check that a valid action has been chosen.
  DCHECK(file_selector_mode);
  DCHECK(title);

  select_file_dialog_->SelectFile(
      file_selector_mode, l10n_util::GetStringUTF16(title),
      GetDefaultFilepathForPasswordFile(file_type_info.extensions[0][0]),
      &file_type_info, 1, file_type_info.extensions[0][0],
      web_contents->GetTopLevelNativeWindow(), reinterpret_cast<void*>(type));
#endif
}

void PasswordManagerPorter::FileSelected(const base::FilePath& path,
                                         int index,
                                         void* params) {
  switch (reinterpret_cast<uintptr_t>(params)) {
    case PASSWORD_IMPORT:
      ImportPasswordsFromPath(path);
      break;
    case PASSWORD_EXPORT:
      ExportPasswordsToPath(path);
      break;
  }
}

void PasswordManagerPorter::FileSelectionCanceled(void* params) {
  if (reinterpret_cast<uintptr_t>(params) == PASSWORD_EXPORT) {
    exporter_->Cancel();
  }
}

void PasswordManagerPorter::ImportPasswordsFromPath(
    const base::FilePath& path) {
  // Set up a |PasswordImportConsumer| to process each password entry.
  std::unique_ptr<PasswordImportConsumer> form_consumer(
      new PasswordImportConsumer(profile_));
  password_manager::PasswordImporter::Import(
      path, base::BindOnce(&PasswordImportConsumer::ConsumePassword,
                           std::move(form_consumer)));
}

void PasswordManagerPorter::ExportPasswordsToPath(const base::FilePath& path) {
  exporter_->SetDestination(path);
}
