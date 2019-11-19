// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/ui/main_dialog_api.h"

#include "chrome/chrome_cleaner/os/file_path_set.h"
#include "chrome/chrome_cleaner/pup_data/pup_cleaner_util.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"

namespace chrome_cleaner {

void MainDialogAPI::ConfirmCleanupIfNeeded(
    const std::vector<UwSId>& found_pups,
    scoped_refptr<DigestVerifier> digest_verifier) {
  FilePathSet collected_pup_files;
  CollectRemovablePupFiles(found_pups, digest_verifier, &collected_pup_files);
  if (collected_pup_files.empty()) {
    LOG(ERROR) << "No removable files detected during the scan";
    NoPUPsFound();
    return;
  }

  std::vector<base::string16> registry_keys;
  for (const auto& pup_id : found_pups) {
    const auto* pup = PUPData::GetPUP(pup_id);
    for (const auto& registry_footprint : pup->expanded_registry_footprints)
      registry_keys.push_back(registry_footprint.key_path.FullPath());
  }

  ConfirmCleanup(found_pups, collected_pup_files, registry_keys);
}

}  // namespace chrome_cleaner
