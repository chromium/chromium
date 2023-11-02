// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_ELF_THIRD_PARTY_DLLS_LOGS_H_
#define CHROME_CHROME_ELF_THIRD_PARTY_DLLS_LOGS_H_

#include <windows.h>

#include <stdint.h>

#include <string>

#include "chrome/chrome_elf/third_party_dlls/public_api.h"
#include "chrome/chrome_elf/third_party_dlls/status_codes.h"

namespace third_party_dlls {

// Adds a module load attempt to the internal load log.
// - |log_type| indicates the type of logging.
// - |image_size| and |time_date_stamp| from the PE headers.
// - |full_image_path| indicates the full path of the loaded image.
// - Note: if there was any failure retrieving the full path, pass at least the
//   basename for |full_image_path|.
void LogLoadAttempt(LogType log_type,
                    uint32_t image_size,
                    uint32_t time_date_stamp,
                    const std::string& full_image_path);

// Initialize internal logs.
ThirdPartyStatus InitLogs();

// Removes initialization for use by tests, or cleanup on failure.
void DeinitLogs();

}  // namespace third_party_dlls

#endif  // CHROME_CHROME_ELF_THIRD_PARTY_DLLS_LOGS_H_
