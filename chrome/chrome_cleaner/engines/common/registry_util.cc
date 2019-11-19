// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/common/registry_util.h"

#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "sandbox/win/src/win_utils.h"

using chrome_cleaner::String16EmbeddedNulls;

namespace chrome_cleaner_sandbox {

void InitUnicodeString(UNICODE_STRING* unicode_string,
                       std::vector<wchar_t>* data) {
  DCHECK(data && !data->empty() && data->back() == L'\0');
  // As per
  // https://msdn.microsoft.com/en-us/library/windows/hardware/ff564879.aspx
  // Length is the length of the string in BYTES, NOT including the
  // terminating NULL char, MaximumLength is the length of the string in BYTES
  // INCLUDING the terminating NULL char.
  unicode_string->Length =
      static_cast<USHORT>((data->size() - 1) * sizeof(wchar_t));
  unicode_string->MaximumLength =
      static_cast<USHORT>(unicode_string->Length + sizeof(wchar_t));
  unicode_string->Buffer = const_cast<wchar_t*>(data->data());
}

bool ValidateRegistryValueChange(const String16EmbeddedNulls& old_value,
                                 const String16EmbeddedNulls& new_value) {
  size_t new_cursor = 0;
  size_t old_cursor = 0;
  while (new_cursor < new_value.size()) {
    while (old_cursor < old_value.size() &&
           new_value.data()[new_cursor] != old_value.data()[old_cursor]) {
      old_cursor++;
    }

    if (old_cursor >= old_value.size())
      return false;

    old_cursor++;
    new_cursor++;
  }

  return true;
}

NtRegistryParamError ValidateNtRegistryValue(
    const String16EmbeddedNulls& param) {
  if (param.size() >= kMaxRegistryParamLength)
    return NtRegistryParamError::TooLong;
  return NtRegistryParamError::None;
}

NtRegistryParamError ValidateNtRegistryNullTerminatedParam(
    const String16EmbeddedNulls& param) {
  if (param.size() == 0)
    return NtRegistryParamError::ZeroLength;
  if (param.size() >= kMaxRegistryParamLength)
    return NtRegistryParamError::TooLong;
  if (param.data()[param.size() - 1] != L'\0')
    return NtRegistryParamError::NotNullTerminated;
  return NtRegistryParamError::None;
}

NtRegistryParamError ValidateNtRegistryKey(const String16EmbeddedNulls& key) {
  NtRegistryParamError error = ValidateNtRegistryNullTerminatedParam(key);
  if (error != NtRegistryParamError::None)
    return error;

  if (key.data()[0] == L'\\') {
    if (!base::StartsWith(key.CastAsStringPiece16(), L"\\Registry\\",
                          base::CompareCase::INSENSITIVE_ASCII)) {
      return NtRegistryParamError::PathOutsideRegistry;
    }
  }

  // TODO(joenotcharles): Ban .. to prevent path traversal. This is not
  // critical as all the native registry functions call NtOpenKey, which will
  // return an error for keys outside \Registry, but it should be checked for
  // defense in depth.
  return NtRegistryParamError::None;
}

base::string16 FormatNtRegistryMemberForLogging(
    const String16EmbeddedNulls& key) {
  switch (ValidateNtRegistryKey(key)) {
    case NtRegistryParamError::NullParam:
      return L"(null)";
    case NtRegistryParamError::ZeroLength:
      return L"(empty key)";
    case NtRegistryParamError::TooLong:
      return L"(excessively long key)";
    default:
      // Replace null chars with 0s for printing.
      base::string16 str(key.CastAsStringPiece16());
      base::ReplaceChars(str, base::StringPiece16(L"\0", 1), L"\\0", &str);
      return str;
  }
}

std::ostream& operator<<(std::ostream& os, NtRegistryParamError param_error) {
  switch (param_error) {
    case NtRegistryParamError::None:
      os << "parameter is OK";
      break;
    case NtRegistryParamError::NullParam:
      os << "parameter is NULL";
      break;
    case NtRegistryParamError::ZeroLength:
      os << "parameter has length 0";
      break;
    case NtRegistryParamError::TooLong:
      os << "parameter has length above the maximum";
      break;
    case NtRegistryParamError::NotNullTerminated:
      os << "parameter is not NULL terminated";
      break;
    case NtRegistryParamError::PathOutsideRegistry:
      os << "key path is not rooted under \\Registry";
      break;
    default:
      NOTREACHED();
      break;
  }
  return os;
}

NTSTATUS NativeCreateKey(HANDLE parent_key,
                         std::vector<wchar_t>* key_name,
                         HANDLE* out_handle,
                         ULONG* out_disposition) {
  DCHECK(key_name && !key_name->empty() && key_name->back() == L'\0');
  DCHECK(out_handle);
  *out_handle = INVALID_HANDLE_VALUE;

  static NtCreateKeyFunction NtCreateKey = nullptr;
  if (!NtCreateKey)
    ResolveNTFunctionPtr("NtCreateKey", &NtCreateKey);

  // Set up a key with an embedded null.
  UNICODE_STRING uni_name = {0};
  InitUnicodeString(&uni_name, key_name);

  OBJECT_ATTRIBUTES obj_attr = {0};
  InitializeObjectAttributes(&obj_attr, &uni_name, OBJ_CASE_INSENSITIVE,
                             parent_key, /*SecurityDescriptor=*/NULL);

  return NtCreateKey(out_handle, KEY_ALL_ACCESS, &obj_attr, /*TitleIndex=*/0,
                     /*Class=*/nullptr, REG_OPTION_NON_VOLATILE,
                     out_disposition);
}

NTSTATUS NativeOpenKey(HANDLE parent_key,
                       const String16EmbeddedNulls& key_name,
                       uint32_t dw_access,
                       HANDLE* out_handle) {
  static NtOpenKeyFunction NtOpenKey = nullptr;
  if (!NtOpenKey)
    ResolveNTFunctionPtr("NtOpenKey", &NtOpenKey);

  // Set up a key with an embedded null.
  std::vector<wchar_t> key_name_buffer(key_name.data());
  UNICODE_STRING uni_name = {0};
  InitUnicodeString(&uni_name, &key_name_buffer);

  OBJECT_ATTRIBUTES obj_attr = {0};
  InitializeObjectAttributes(&obj_attr, &uni_name, OBJ_CASE_INSENSITIVE,
                             parent_key, /*SecurityDescriptor=*/NULL);

  return NtOpenKey(out_handle, dw_access, &obj_attr);
}

NTSTATUS NativeSetValueKey(HANDLE key,
                           const String16EmbeddedNulls& value_name,
                           ULONG type,
                           const String16EmbeddedNulls& value) {
  static NtSetValueKeyFunction NtSetValueKey = nullptr;
  if (!NtSetValueKey)
    ResolveNTFunctionPtr("NtSetValueKey", &NtSetValueKey);

  UNICODE_STRING uni_name = {};
  std::vector<wchar_t> value_name_buffer(value_name.data());
  if (value_name.size()) {
    InitUnicodeString(&uni_name, &value_name_buffer);
  }

  std::vector<wchar_t> value_buffer(value.data());

  // The astute reader will notice here that we pass a zero'ed out
  // UNICODE_STRING instead of a NULL pointer in the second parameter to set the
  // default value, which is not consistent with the MSDN documentation for
  // ZwSetValueKey. Afaict, the documentation is incorrect, calling this with a
  // NULL pointer returns access denied.
  return NtSetValueKey(
      key, &uni_name, /*TitleIndex=*/0, type,
      reinterpret_cast<void*>(value_buffer.data()),
      base::checked_cast<ULONG>(value_buffer.size() * sizeof(wchar_t)));
}

NTSTATUS NativeQueryValueKey(HANDLE key,
                             const String16EmbeddedNulls& value_name,
                             ULONG* out_type,
                             String16EmbeddedNulls* out_value) {
  // Figure out a) if the value exists and b) what the type of the value is.
  static NtQueryValueKeyFunction NtQueryValueKey = nullptr;
  if (!NtQueryValueKey)
    ResolveNTFunctionPtr("NtQueryValueKey", &NtQueryValueKey);

  UNICODE_STRING uni_name = {};
  std::vector<wchar_t> value_name_buffer(value_name.data());
  if (value_name.size()) {
    InitUnicodeString(&uni_name, &value_name_buffer);
  }

  ULONG size_needed = 0;

  // The astute reader will notice here that we pass a zero'ed out
  // UNICODE_STRING instead of a NULL pointer in the second parameter to set the
  // default value, which is not consistent with the MSDN documentation for
  // ZwQueryValueKey. Afaict, the documentation is incorrect, calling this with
  // a NULL pointer returns access denied.
  NTSTATUS status = NtQueryValueKey(key, &uni_name, KeyValueFullInformation,
                                    nullptr, 0, &size_needed);
  if (status != STATUS_BUFFER_TOO_SMALL || size_needed == 0) {
    LOG(ERROR) << "Call to NtQueryValueKey to query size failed. Returned: "
               << std::hex << status;
    return false;
  }

  std::vector<BYTE> buffer(size_needed);
  ULONG bytes_read = 0;
  status = NtQueryValueKey(key, &uni_name, KeyValueFullInformation,
                           reinterpret_cast<void*>(buffer.data()), size_needed,
                           &bytes_read);
  if (status != STATUS_SUCCESS || bytes_read != size_needed) {
    LOG(ERROR) << "Call to NtQueryValueKey failed.  Returned: " << std::hex
               << status;
    return false;
  }

  KEY_VALUE_FULL_INFORMATION* full_information =
      reinterpret_cast<KEY_VALUE_FULL_INFORMATION*>(buffer.data());

  if (out_type)
    *out_type = full_information->Type;
  if (out_value) {
    // Make sure that the length is a multiple of the width of a wchar_t to
    // avoid alignment errors.
    if (full_information->DataLength % sizeof(wchar_t) != 0) {
      LOG(ERROR) << "Mis-aligned size reading registry value.";
      return false;
    }

    // Explanation for fiery mess of casts: the value data from NtQueryValueKey
    // is in a memory block allocated right at the end of the
    // KEY_VALUE_FULL_INFORMATION structure. The DataOffset member is the offset
    // in BYTES of the start of the value data.
    // The inner reinterpret_cast ensures that the pointer arithmetic uses
    // BYTE widths.
    // The outer reinterpret_cast then treats that address as a wchar_t*,
    // which is safe due to the alignment check above.
    const wchar_t* data_start =
        reinterpret_cast<wchar_t*>(reinterpret_cast<BYTE*>(full_information) +
                                   full_information->DataOffset);
    *out_value = String16EmbeddedNulls(
        data_start, full_information->DataLength / sizeof(wchar_t));
  }

  return true;
}

NTSTATUS NativeDeleteKey(HANDLE handle) {
  static NtDeleteKeyFunction NtDeleteKey = nullptr;
  if (!NtDeleteKey)
    ResolveNTFunctionPtr("NtDeleteKey", &NtDeleteKey);
  return NtDeleteKey(handle);
}

}  // namespace chrome_cleaner_sandbox
