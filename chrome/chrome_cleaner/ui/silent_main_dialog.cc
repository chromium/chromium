// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/ui/silent_main_dialog.h"

#include "base/check.h"

namespace chrome_cleaner {

SilentMainDialog::SilentMainDialog(MainDialogDelegate* delegate)
    : MainDialogAPI(delegate) {
  DCHECK(delegate);
}

SilentMainDialog::~SilentMainDialog() {}

bool SilentMainDialog::Create() {
  return true;
}

void SilentMainDialog::NoPUPsFound() {
  delegate()->OnClose();
}

void SilentMainDialog::ConfirmCleanup(
    const std::vector<UwSId>& found_pups,
    const FilePathSet& files_to_remove,
    const std::vector<std::wstring>& registry_keys) {
  delegate()->AcceptedCleanup(true);
}

void SilentMainDialog::CleanupDone(ResultCode cleanup_result) {
  delegate()->OnClose();
}

void SilentMainDialog::Close() {
  delegate()->OnClose();
}

}  // namespace chrome_cleaner
