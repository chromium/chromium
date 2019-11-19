// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/pup_data/pup_cleaner_util.h"

#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "chrome/chrome_cleaner/os/file_remover.h"
#include "chrome/chrome_cleaner/os/layered_service_provider_wrapper.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"

namespace chrome_cleaner {

bool CollectRemovablePupFiles(const std::vector<UwSId>& pup_ids,
                              scoped_refptr<DigestVerifier> digest_verifier,
                              FilePathSet* pup_files) {
  bool valid_removal = true;

  auto lsp = chrome_cleaner::LayeredServiceProviderWrapper();
  chrome_cleaner::FileRemover file_remover(digest_verifier,
                                           /*archiver=*/nullptr,
                                           lsp,
                                           base::DoNothing());

  for (const auto& pup_id : pup_ids) {
    size_t added_pup_files_size = 0;
    const auto* pup = PUPData::GetPUP(pup_id);

    for (const auto& file_path : pup->expanded_disk_footprints.file_paths()) {
      // Verify that files can be deleted.
      if (file_remover.CanRemove(file_path) ==
          FileRemoverAPI::DeletionValidationStatus::ALLOWED) {
        pup_files->Insert(file_path);
        ++added_pup_files_size;
      }
    }
  }

  return valid_removal;
}

}  // namespace chrome_cleaner
