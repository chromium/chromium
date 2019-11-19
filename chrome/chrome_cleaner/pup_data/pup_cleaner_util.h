// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_PUP_DATA_PUP_CLEANER_UTIL_H_
#define CHROME_CHROME_CLEANER_PUP_DATA_PUP_CLEANER_UTIL_H_

#include <vector>

#include "chrome/chrome_cleaner/constants/uws_id.h"
#include "chrome/chrome_cleaner/logging/proto/shared_data.pb.h"
#include "chrome/chrome_cleaner/os/digest_verifier.h"
#include "chrome/chrome_cleaner/os/file_path_set.h"

namespace chrome_cleaner {

// Collects removable files in the disk footprints of the PUPs identified in
// |pup_ids| and adds their full paths to |pup_files|. Returns true when it is
// safe to remove files in |pup_files|.
bool CollectRemovablePupFiles(const std::vector<UwSId>& pup_ids,
                              scoped_refptr<DigestVerifier> digest_verifier,
                              FilePathSet* pup_files);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_PUP_DATA_PUP_CLEANER_UTIL_H_
