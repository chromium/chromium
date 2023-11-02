// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_UI_SILENT_MAIN_DIALOG_H_
#define CHROME_CHROME_CLEANER_UI_SILENT_MAIN_DIALOG_H_

#include "chrome/chrome_cleaner/ui/main_dialog_api.h"

namespace chrome_cleaner {

// Silent version of MainDialogAPI, to be used in end to end tests. It silently
// accepts cleanup confirmations, and automatically quits when done messages
// are shown.
class SilentMainDialog : public MainDialogAPI {
 public:
  // The given delegate must outlive the SilentMainDialog.
  explicit SilentMainDialog(MainDialogDelegate* delegate);
  ~SilentMainDialog() override;

  // MainDialogAPI overrides.
  bool Create() override;
  void NoPUPsFound() override;
  void CleanupDone(ResultCode cleanup_result) override;
  void Close() override;

 protected:
  void ConfirmCleanup(const std::vector<UwSId>& found_pups,
                      const FilePathSet& files_to_remove,
                      const std::vector<std::wstring>& registry_keys) override;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_UI_SILENT_MAIN_DIALOG_H_
