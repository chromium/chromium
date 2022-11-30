// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_COMPONENTS_RESET_SHORTCUTS_COMPONENT_H_
#define CHROME_CHROME_CLEANER_COMPONENTS_RESET_SHORTCUTS_COMPONENT_H_

#include <vector>

#include "chrome/chrome_cleaner/components/component_api.h"
#include "chrome/chrome_cleaner/parsers/shortcut_parser/broker/sandboxed_shortcut_parser.h"

namespace chrome_cleaner {

// This class manages the resetting of shortcuts.
class ResetShortcutsComponent : public ComponentAPI {
 public:
  explicit ResetShortcutsComponent(ShortcutParserAPI* shortcut_parser);
  ~ResetShortcutsComponent() override;

  // ComponentAPI methods.
  void PreScan() override;
  void PostScan(const std::vector<UwSId>& found_pups) override;
  void PreCleanup() override;
  void PostCleanup(ResultCode result_code, RebooterAPI* rebooter) override;
  void PostValidation(ResultCode result_code) override;
  void OnClose(ResultCode result_code) override;

  // Get the list of shortcuts to reset.
  std::vector<ShortcutInformation> GetShortcuts();
  // Find and reset shortcuts while preserving the icon location and some
  // command line flags.
  void FindAndResetShortcuts();

  void SetShortcutPathsToExploreForTesting(
      const std::vector<base::FilePath>& fake_shortcut_location_paths_);

  void SetChromeExeFilePathSetForTesting(
      const FilePathSet& fake_chrome_exe_working_dirs);

 private:
  ShortcutParserAPI* shortcut_parser_;
  std::vector<base::FilePath> shortcut_paths_to_explore_;
  FilePathSet chrome_exe_file_path_set_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_COMPONENTS_RESET_SHORTCUTS_COMPONENT_H_
