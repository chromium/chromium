// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/scanning/scanning_handler.h"

#include "base/strings/string16.h"
#include "base/values.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace {

constexpr char kBaseName[] = "baseName";
constexpr char kFilePath[] = "filePath";

// Uses the full filepath and the base directory (lowest level directory in the
// filepath, used to display in the UI) to create a Value object to return to
// the Scanning UI.
base::Value CreateSelectedPathValue(const base::FilePath& path) {
  base::Value selected_path(base::Value::Type::DICTIONARY);
  selected_path.SetStringKey(kBaseName, path.BaseName().value());
  selected_path.SetStringKey(kFilePath, path.value());
  return selected_path;
}

}  // namespace

namespace chromeos {

ScanningHandler::ScanningHandler(
    const SelectFilePolicyCreator& select_file_policy_creator)
    : select_file_policy_creator_(select_file_policy_creator) {}

ScanningHandler::~ScanningHandler() = default;

void ScanningHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "initialize", base::BindRepeating(&ScanningHandler::HandleInitialize,
                                        base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "requestScanToLocation",
      base::BindRepeating(&ScanningHandler::HandleRequestScanToLocation,
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
      ui::SelectFileDialog::SELECT_FOLDER, base::string16() /* title */,
      base::FilePath() /* default_path */, nullptr /* file_types */,
      0 /* file_type_index */,
      base::FilePath::StringType() /* default_extension */, owning_window,
      nullptr /* params */);
}

void ScanningHandler::FileSelected(const base::FilePath& path,
                                   int index,
                                   void* params) {
  if (IsJavascriptAllowed()) {
    ResolveJavascriptCallback(base::Value(scan_location_callback_id_),
                              CreateSelectedPathValue(path));
  }
}

void ScanningHandler::FileSelectionCanceled(void* params) {
  if (IsJavascriptAllowed()) {
    ResolveJavascriptCallback(base::Value(scan_location_callback_id_),
                              CreateSelectedPathValue(base::FilePath()));
  }
}

void ScanningHandler::SetWebUIForTest(content::WebUI* web_ui) {
  set_web_ui(web_ui);
}

}  // namespace chromeos
