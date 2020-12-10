// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SCANNING_SCANNING_HANDLER_H_
#define CHROMEOS_COMPONENTS_SCANNING_SCANNING_HANDLER_H_

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace base {
class ListValue;
}  // namespace base

namespace content {
class WebContents;
}  // namespace content

namespace chromeos {

class ScanningPathsProvider;

// ChromeOS Scanning app UI handler.
class ScanningHandler : public content::WebUIMessageHandler,
                        public ui::SelectFileDialog::Listener {
 public:
  using SelectFilePolicyCreator =
      base::RepeatingCallback<std::unique_ptr<ui::SelectFilePolicy>(
          content::WebContents*)>;
  using OpenFilesAppFunction =
      base::RepeatingCallback<bool(content::WebUI*, const base::FilePath&)>;

  ScanningHandler(
      const SelectFilePolicyCreator& select_file_policy_creator,
      std::unique_ptr<ScanningPathsProvider> scanning_paths_provider,
      OpenFilesAppFunction open_files_app_fn);
  ~ScanningHandler() override;

  ScanningHandler(const ScanningHandler&) = delete;
  ScanningHandler& operator=(const ScanningHandler&) = delete;

  // WebUIMessageHandler:
  void RegisterMessages() override;

  // SelectFileDialog::Listener:
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void FileSelectionCanceled(void* params) override;

  base::Value CreateSelectedPathValue(const base::FilePath& path);

  // Adds to map of string IDs for pluralization.
  void AddStringToPluralMap(const std::string& name, int id);

  void SetWebUIForTest(content::WebUI* web_ui);

 private:
  // Initializes Javascript.
  void HandleInitialize(const base::ListValue* args);

  // Opens the select dialog for the user to choose the directory to save
  // completed scans.
  void HandleRequestScanToLocation(const base::ListValue* args);

  // Opens the Files app to the show the saved scan file.
  void HandleShowFileInLocation(const base::ListValue* args);

  // Returns a localized, pluralized string.
  void HandleGetPluralString(const base::ListValue* args);

  SelectFilePolicyCreator select_file_policy_creator_;

  std::string scan_location_callback_id_;

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  // Provides FilePath util for converting a FilePath base name.
  std::unique_ptr<ScanningPathsProvider> scanning_paths_provider_;

  // Opens Files app to the desired file location.
  OpenFilesAppFunction open_files_app_fn_;

  std::map<std::string, int> string_id_map_;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SCANNING_SCANNING_HANDLER_H_
