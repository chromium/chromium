// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_ELF_THIRD_PARTY_DLLS_PACKED_LIST_FILE_H_
#define CHROME_CHROME_ELF_THIRD_PARTY_DLLS_PACKED_LIST_FILE_H_

#include <string>

#include "chrome/chrome_elf/sha1/sha1.h"
#include "chrome/chrome_elf/third_party_dlls/status_codes.h"

namespace third_party_dlls {

// Look up a binary based on the required data points.
// - Returns true if match found in the list.
bool IsModuleListed(const elf_sha1::Digest& basename_hash,
                    const elf_sha1::Digest& fingerprint_hash);

// Get the full path of the blacklist file used.
std::wstring GetBlFilePathUsed();

// Initialize internal module list from file.
// - NOTE: Caller should use IsStatusCodeSuccessful() on return value.
ThirdPartyStatus InitFromFile();

// Centralized utility to determine if a status code from this module should be
// treated as non-fatal.
bool IsStatusCodeSuccessful(ThirdPartyStatus code);

// Removes initialization for use by tests, or cleanup on failure.
void DeinitFromFile();

// Overrides the blacklist path for use by tests.
void OverrideFilePathForTesting(const std::wstring& new_bl_path);

}  // namespace third_party_dlls

#endif  // CHROME_CHROME_ELF_THIRD_PARTY_DLLS_PACKED_LIST_FILE_H_
