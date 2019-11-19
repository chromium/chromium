// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_COMMON_REGISTRY_UTIL_H_
#define CHROME_CHROME_CLEANER_ENGINES_COMMON_REGISTRY_UTIL_H_

#include <limits>
#include <ostream>
#include <vector>

#include "chrome/chrome_cleaner/strings/string16_embedded_nulls.h"
#include "sandbox/win/src/nt_internals.h"

namespace chrome_cleaner_sandbox {

// Possible errors in native registry parameter strings (keys or values). Note
// that these strings allow embedded nulls, but must also be null terminated.
enum class NtRegistryParamError {
  None,
  NullParam,
  ZeroLength,
  TooLong,
  NotNullTerminated,
  PathOutsideRegistry,  // Only valid for keys.
};

// The maximum length the sandboxed scanner/cleaner APIs will accept for a
// registry key or value.
constexpr uint32_t kMaxRegistryParamLength =
    std::numeric_limits<uint16_t>::max();

// Initializes the given |unicode_string| with the character data stored in
// |data|.
// It is assumed that |data| is a null-terminated list of wchar_ts. |data| may
// also contain embedded NULL chars, making this a convenient alternative to
// RtlInitUnicodeString if you wish to handle data with embedded NULLs.
// It is important that whatever |data| points to outlives any usage
// of |unicode_string| since |unicode_string| will point into |data|'s buffer.
// It is also important that |data| not be modified after a call to this
// function.
void InitUnicodeString(UNICODE_STRING* unicode_string,
                       std::vector<wchar_t>* data);

// Returns true if |new_value| can be arrived at solely by deleting 0 or more
// characters from |old_value|.
bool ValidateRegistryValueChange(
    const chrome_cleaner::String16EmbeddedNulls& old_value,
    const chrome_cleaner::String16EmbeddedNulls& new_value);

// Checks for errors in a value parameter for a native registry function.
NtRegistryParamError ValidateNtRegistryValue(
    const chrome_cleaner::String16EmbeddedNulls& param);

// Checks for errors in a native registry function parameter that is expected
// to be NULL-terminated (keys and value names).
NtRegistryParamError ValidateNtRegistryNullTerminatedParam(
    const chrome_cleaner::String16EmbeddedNulls& param);

// Checks for errors in a native registry key path: all the errors detected by
// ValidateNtRegistryParam, plus if it's an absolute path it must be under
// \Registry.
NtRegistryParamError ValidateNtRegistryKey(
    const chrome_cleaner::String16EmbeddedNulls& key);

// Format a native registry key, value or value name (which may contain
// embedded NULLs) for logging.
base::string16 FormatNtRegistryMemberForLogging(
    const chrome_cleaner::String16EmbeddedNulls& key);

// Format NtRegistryParamError and write it to a stream for logging.
std::ostream& operator<<(std::ostream& os, NtRegistryParamError param_error);

// |key_name| must be a null-terminated list of wchar_ts, that may also include
// embedded nulls. |key_name| is not a const& since under the hood the native
// functions take non-const pointers.
NTSTATUS NativeCreateKey(HANDLE parent_key,
                         std::vector<wchar_t>* key_name,
                         HANDLE* out_handle,
                         ULONG* out_disposition);

// |key_name| must be a null-terminated string.
NTSTATUS NativeOpenKey(HANDLE parent_key,
                       const chrome_cleaner::String16EmbeddedNulls& key_name,
                       uint32_t dw_access,
                       HANDLE* out_handle);

// |value_name| and |value| must be null-terminated strings. |value_name| may be
// empty to signify setting |key|'s default value. |type| is one of the registry
// value types described here:
// https://msdn.microsoft.com/en-us/library/windows/desktop/ms724884.aspx
NTSTATUS NativeSetValueKey(
    HANDLE key,
    const chrome_cleaner::String16EmbeddedNulls& value_name,
    ULONG type,
    const chrome_cleaner::String16EmbeddedNulls& value);

// Retrieves the type and data of the value under |registry_handle| specified by
// |value_name| and places it them in |out_type| and |out_value|. Either or both
// of |out_type| and |out_value| may be null in which case they won't be
// returned. Returns true on success, false otherwise.
NTSTATUS NativeQueryValueKey(
    HANDLE key,
    const chrome_cleaner::String16EmbeddedNulls& value_name,
    ULONG* out_type,
    chrome_cleaner::String16EmbeddedNulls* out_value);

NTSTATUS NativeDeleteKey(HANDLE handle);

}  // namespace chrome_cleaner_sandbox

#endif  // CHROME_CHROME_CLEANER_ENGINES_COMMON_REGISTRY_UTIL_H_
