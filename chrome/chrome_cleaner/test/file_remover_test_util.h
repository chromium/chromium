// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_TEST_FILE_REMOVER_TEST_UTIL_H_
#define CHROME_CHROME_CLEANER_TEST_FILE_REMOVER_TEST_UTIL_H_

#include "base/files/file_path.h"
#include "chrome/chrome_cleaner/os/file_remover_api.h"

namespace chrome_cleaner {

void VerifyRemoveNowSuccess(const base::FilePath& path,
                            FileRemoverAPI* remover);

void VerifyRemoveNowFailure(const base::FilePath& path,
                            FileRemoverAPI* remover);

void VerifyRegisterPostRebootRemovalSuccess(const base::FilePath& path,
                                            FileRemoverAPI* remover);

void VerifyRegisterPostRebootRemovalFailure(const base::FilePath& path,
                                            FileRemoverAPI* remover);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_TEST_FILE_REMOVER_TEST_UTIL_H_
