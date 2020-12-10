// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/scanning/scanning_handler.h"

#include "base/strings/string16.h"
#include "base/values.h"
#include "chromeos/components/scanning/scanning_paths_provider.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace {

constexpr char kBaseName[] = "baseName";
constexpr char kFilePath[] = "filePath";

}  // namespace

namespace chromeos {

ScanningHandler::ScanningHandler(
    const SelectFilePolicyCreator& select_file_policy_creator,
    std::unique_ptr<ScanningPathsProvider> scanning_paths_provider,
    OpenFilesAppFunction open_files_app_fn)
    : select_file_policy_creator_(select_file_policy_creator),
      scanning_paths_provider_(std::move(scanning_paths_provider)),
      open_files_app_fn_(std::move(open_files_app_fn)) {}

ScanningHandler::~ScanningHandler() = default;

void ScanningHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "initialize", base::BindRepeating(&ScanningHandler::HandleInitialize,
                                        base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "requestScanToLocation",
      base::BindRepeating(&ScanningHandler::HandleRequestScanToLocation,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "showFileInLocation",
      base::BindRepeating(&ScanningHandler::HandleShowFileInLocation,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getPluralString",
      base::BindRepeating(&ScanningHandler::HandleGetPluralString,
                          base::Unretained(this)));
}

void ScanningHandler::HandleInitialize(const base::ListValue* args) {
  DCHECK(args && args->empty());
  AllowJavascript();
}

void ScanningHandler::HandleRequestScanToLocation(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  scan_location_callback_id_ = args->GetList()[0].GetString();

  content::WebContents* web_contents = web_ui()->GetWebContents();
  gfx::NativeWindow owning_window =
      web_contents ? web_contents->GetTopLevelNativeWindow()
                   : gfx::kNullNativeWindow;
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, select_file_policy_creator_.Run(web_contents));
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_FOLDER,
      l10n_util::GetStringUTF16(IDS_SCANNING_APP_SELECT_DIALOG_TITLE),
      base::FilePath() /* default_path */, nullptr /* file_types */,
      0 /* file_type_index */,
      base::FilePath::StringType() /* default_extension */, owning_window,
      nullptr /* params */);
}

void ScanningHandler::HandleShowFileInLocation(const base::ListValue* args) {
  if (!IsJavascriptAllowed())
    return;

  CHECK_EQ(2U, args->GetSize());
  const std::string callback = args->GetList()[0].GetString();
  const base::FilePath file_location(args->GetList()[1].GetString());
  const bool file_opened = open_files_app_fn_.Run(web_ui(), file_location);
  ResolveJavascriptCallback(base::Value(callback), base::Value(file_opened));
}

void ScanningHandler::FileSelected(const base::FilePath& path,
                                   int index,
                                   void* params) {
  if (!IsJavascriptAllowed())
    return;

  ResolveJavascriptCallback(base::Value(scan_location_callback_id_),
                            CreateSelectedPathValue(path));
}

void ScanningHandler::FileSelectionCanceled(void* params) {
  if (!IsJavascriptAllowed())
    return;

  ResolveJavascriptCallback(base::Value(scan_location_callback_id_),
                            CreateSelectedPathValue(base::FilePath()));
}

void ScanningHandler::AddStringToPluralMap(const std::string& name,
                                           int string_id) {
  string_id_map_[name] = string_id;
}

void ScanningHandler::HandleGetPluralString(const base::ListValue* args) {
  if (!IsJavascriptAllowed())
    return;

  CHECK_EQ(3U, args->GetSize());
  const std::string callback = args->GetList()[0].GetString();
  const std::string name = args->GetList()[1].GetString();
  const int count = args->GetList()[2].GetInt();

  const base::string16 localized_string = l10n_util::GetPluralStringFUTF16(
      string_id_map_.find(name)->second, count);
  ResolveJavascriptCallback(base::Value(callback),
                            base::Value(localized_string));
}

// Uses the full filepath and the base directory (lowest level directory in the
// filepath, used to display in the UI) to create a Value object to return to
// the Scanning UI.
base::Value ScanningHandler::CreateSelectedPathValue(
    const base::FilePath& path) {
  base::Value selected_path(base::Value::Type::DICTIONARY);
  selected_path.SetStringKey(kFilePath, path.value());
  selected_path.SetStringKey(
      kBaseName, scanning_paths_provider_->GetBaseNameFromPath(web_ui(), path));
  return selected_path;
}

void ScanningHandler::SetWebUIForTest(content::WebUI* web_ui) {
  set_web_ui(web_ui);
}

}  // namespace chromeos
