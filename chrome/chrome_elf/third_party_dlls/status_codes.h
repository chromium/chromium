// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_ELF_THIRD_PARTY_DLLS_STATUS_CODES_H_
#define CHROME_CHROME_ELF_THIRD_PARTY_DLLS_STATUS_CODES_H_

#include <stddef.h>

#include <vector>

namespace third_party_dlls {

// Registry value in the main |kThirdPartyRegKeyName| registry key.
// - Of type REG_BINARY, the binary blob will contain 0 to many ThirdPartyStatus
//   codes.
// - Use the utility functions below to manipulate the binary buffer.
extern const wchar_t kStatusCodesRegValue[];

// - Don't change the values of these enums, without careful thought.  They are
//   used in an UMA histogram.
// - "static_cast<int>(ThirdPartyStatus::value)" to access underlying value.
enum class ThirdPartyStatus {
  kSuccess = 0,
  // packed_list_file, non-fatal codes:
  kFilePathNotFoundInRegistry = 1,
  kFileNotFound = 2,
  kFileEmpty = 3,
  kFileArraySizeZero = 4,
  // packed_list_file:
  kFileAccessDenied = 5,
  kFileUnexpectedFailure = 6,
  kFileMetadataReadFailure = 7,
  kFileInvalidFormatVersion = 8,
  kFileArrayTooBig = 9,
  kFileArrayReadFailure = 10,
  kFileArrayNotSorted = 11,
  // logs:
  kLogsCreateMutexFailure = 12,
  // hook:
  kHookInitImportsFailure = 13,
  kHookUnsupportedOs = 14,
  kHookVirtualProtectFailure = 15,
  kHookApplyFailure = 16,
  // status_codes:
  kStatusCodeResetFailure = 17,
  kMaxValue = kStatusCodeResetFailure,
};

// Append a ThirdPartyStatus code to a |kStatusCodesRegValue| REG_BINARY buffer.
void AddStatusCodeToBuffer(ThirdPartyStatus code, std::vector<uint8_t>* buffer);

// Convert a |kStatusCodesRegValue| REG_BINARY buffer to ThirdPartyStatus codes.
void ConvertBufferToStatusCodes(const std::vector<uint8_t>& buffer,
                                std::vector<ThirdPartyStatus>* status_array);

}  // namespace third_party_dlls

#endif  // CHROME_CHROME_ELF_THIRD_PARTY_DLLS_STATUS_CODES_H_
