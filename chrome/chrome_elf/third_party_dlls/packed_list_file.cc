// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_elf/third_party_dlls/packed_list_file.h"

#include <windows.h>

#include <assert.h>
#include <stdio.h>

#include <algorithm>
#include <limits>

#include "chrome/chrome_elf/nt_registry/nt_registry.h"
#include "chrome/chrome_elf/third_party_dlls/packed_list_format.h"
#include "chrome/install_static/install_util.h"

namespace third_party_dlls {
namespace {

// No concern about concurrency control in chrome_elf.
bool g_initialized = false;

// This will hold a packed blacklist module array, read directly from a
// data file during InitFromFile().
PackedListModule* g_bl_module_array = nullptr;
size_t g_bl_module_array_size = 0;

// NOTE: this "global" is only initialized once on first access.
// NOTE: it is wrapped in a function to prevent exit-time dtors.
std::wstring& GetBlFilePath() {
  static std::wstring* const file_path = new std::wstring();
  return *file_path;
}

//------------------------------------------------------------------------------
// Private functions
//------------------------------------------------------------------------------

// Binary predicate compare function for use with
// std::equal_range/std::is_sorted. Must return TRUE if lhs < rhs.
bool HashBinaryPredicate(const PackedListModule& lhs,
                         const PackedListModule& rhs) {
  return lhs.basename_hash < rhs.basename_hash;
}

// Given a file opened for read, pull in the packed list.
ThirdPartyStatus ReadInArray(HANDLE file,
                             size_t* array_size,
                             PackedListModule** array_ptr) {
  PackedListMetadata metadata;
  DWORD bytes_read = 0;

  if (!::ReadFile(file, &metadata, sizeof(PackedListMetadata), &bytes_read,
                  FALSE) ||
      bytes_read != sizeof(PackedListMetadata)) {
    // If |bytes_read| is actually 0, then the file was empty.
    if (!bytes_read)
      return ThirdPartyStatus::kFileEmpty;
    return ThirdPartyStatus::kFileMetadataReadFailure;
  }

  // Careful of versioning.  For now, only support the latest version.
  if (metadata.version != PackedListVersion::kCurrent)
    return ThirdPartyStatus::kFileInvalidFormatVersion;

  *array_size = metadata.module_count;
  // Check for size 0.
  if (!*array_size)
    return ThirdPartyStatus::kFileArraySizeZero;

  // Sanity check the array fits in a DWORD.
  if (*array_size >
      (std::numeric_limits<DWORD>::max() / sizeof(PackedListModule))) {
    assert(false);
    return ThirdPartyStatus::kFileArrayTooBig;
  }

  DWORD buffer_size =
      static_cast<DWORD>(*array_size * sizeof(PackedListModule));
  *array_ptr = reinterpret_cast<PackedListModule*>(new uint8_t[buffer_size]);

  // Read in the array.
  // NOTE: Ignore the rest of the file - other data could be stored at the end.
  if (!::ReadFile(file, *array_ptr, buffer_size, &bytes_read, FALSE) ||
      bytes_read != buffer_size) {
    delete[] * array_ptr;
    *array_ptr = nullptr;
    *array_size = 0;
    return ThirdPartyStatus::kFileArrayReadFailure;
  }

  // Ensure array is sorted (as expected).
  if (!std::is_sorted(*array_ptr, *array_ptr + *array_size,
                      HashBinaryPredicate)) {
    delete[] * array_ptr;
    *array_ptr = nullptr;
    *array_size = 0;
    return ThirdPartyStatus::kFileArrayNotSorted;
  }

  return ThirdPartyStatus::kSuccess;
}

// Reads the path to the blacklist file from the registry into |file_path|.
//
// - Returns false if the value is not found, is not a REG_SZ, or is empty.
bool GetFilePathFromRegistry(std::wstring* file_path) {
  file_path->clear();
  HANDLE key_handle = nullptr;

  if (!nt::CreateRegKey(nt::HKCU,
                        install_static::GetRegistryPath()
                            .append(kThirdPartyRegKeyName)
                            .c_str(),
                        KEY_QUERY_VALUE, &key_handle)) {
    return false;
  }

  bool found = nt::QueryRegValueSZ(key_handle, kBlFilePathRegValue, file_path);
  nt::CloseRegKey(key_handle);

  return found && !file_path->empty();
}

// Open a packed data file.
ThirdPartyStatus OpenDataFile(HANDLE* file_handle) {
  *file_handle = INVALID_HANDLE_VALUE;
  std::wstring& file_path = GetBlFilePath();

  // The path may have been overridden for testing.
  if (file_path.empty() && !GetFilePathFromRegistry(&file_path))
    return ThirdPartyStatus::kFilePathNotFoundInRegistry;

  // See if file exists.  INVALID_HANDLE_VALUE alert!
  *file_handle =
      ::CreateFileW(file_path.c_str(), FILE_READ_DATA,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (*file_handle == INVALID_HANDLE_VALUE) {
    switch (::GetLastError()) {
      case ERROR_FILE_NOT_FOUND:
      case ERROR_PATH_NOT_FOUND:
        return ThirdPartyStatus::kFileNotFound;
      case ERROR_ACCESS_DENIED:
        return ThirdPartyStatus::kFileAccessDenied;
      default:
        return ThirdPartyStatus::kFileUnexpectedFailure;
    }
  }

  return ThirdPartyStatus::kSuccess;
}

// Find the packed blacklist file and read in the array.
ThirdPartyStatus InitInternal() {
  HANDLE handle = INVALID_HANDLE_VALUE;
  ThirdPartyStatus status = OpenDataFile(&handle);
  if (status != ThirdPartyStatus::kSuccess)
    return status;

  status = ReadInArray(handle, &g_bl_module_array_size, &g_bl_module_array);
  ::CloseHandle(handle);

  return status;
}

}  // namespace

//------------------------------------------------------------------------------
// Public defines & functions
//------------------------------------------------------------------------------

bool IsModuleListed(const elf_sha1::Digest& basename_hash,
                    const elf_sha1::Digest& fingerprint_hash) {
  assert(g_initialized);

  if (!g_bl_module_array_size)
    return false;

  PackedListModule target = {};
  target.basename_hash = basename_hash;
  target.code_id_hash = fingerprint_hash;

  // Binary search for primary hash (basename).  There can be more than one
  // match.
  auto pair = std::equal_range(g_bl_module_array,
                               g_bl_module_array + g_bl_module_array_size,
                               target, HashBinaryPredicate);

  // Search for secondary hash.
  for (PackedListModule* i = pair.first; i != pair.second; ++i) {
    if (target.code_id_hash == i->code_id_hash)
      return true;
  }

  // No match.
  return false;
}

std::wstring GetBlFilePathUsed() {
  assert(g_initialized);
  return GetBlFilePath();
}

ThirdPartyStatus InitFromFile() {
  // Debug check: InitFromFile should not be called more than once.
  assert(!g_initialized);

  ThirdPartyStatus status = InitInternal();

  if (IsStatusCodeSuccessful(status))
    g_initialized = true;

  return status;
}

bool IsStatusCodeSuccessful(ThirdPartyStatus code) {
  if (code == ThirdPartyStatus::kSuccess ||
      code == ThirdPartyStatus::kFilePathNotFoundInRegistry ||
      code == ThirdPartyStatus::kFileNotFound ||
      code == ThirdPartyStatus::kFileEmpty ||
      code == ThirdPartyStatus::kFileArraySizeZero) {
    return true;
  }

  return false;
}

void DeinitFromFile() {
  if (!g_initialized)
    return;

  delete[] g_bl_module_array;
  g_bl_module_array = nullptr;
  g_bl_module_array_size = 0;
  GetBlFilePath().clear();

  g_initialized = false;
}

void OverrideFilePathForTesting(const std::wstring& new_bl_path) {
  GetBlFilePath().assign(new_bl_path);
}

}  // namespace third_party_dlls
