// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/test/test_extensions.h"

#include <string>

#include "base/files/file.h"
#include "base/files/file_util.h"

namespace chrome_cleaner {

TestRegistryEntry::TestRegistryEntry(HKEY hkey,
                                     const std::wstring& path,
                                     const std::wstring& name,
                                     const std::wstring& value)
    : hkey(hkey), path(path), name(name), value(value) {}
TestRegistryEntry::TestRegistryEntry(const TestRegistryEntry& other) = default;
TestRegistryEntry& TestRegistryEntry::operator=(
    const TestRegistryEntry& other) = default;

bool CreateProfileWithExtensionAndFiles(
    const base::FilePath& profile_path,
    const std::wstring& extension_id,
    const std::vector<std::wstring>& extension_files) {
  if (!base::CreateDirectory(profile_path))
    return false;

  base::FilePath extensions_folder_path = profile_path.Append(L"Extensions");
  if (!base::CreateDirectory(extensions_folder_path))
    return false;

  base::FilePath extension_path = extensions_folder_path.Append(extension_id);
  if (!base::CreateDirectory(extension_path))
    return false;

  for (const std::wstring& file_name : extension_files) {
    base::File extension_file(
        extension_path.Append(file_name),
        base::File::Flags::FLAG_CREATE | base::File::Flags::FLAG_READ);

    if (!extension_file.IsValid())
      return false;
  }

  return true;
}

}  // namespace chrome_cleaner
