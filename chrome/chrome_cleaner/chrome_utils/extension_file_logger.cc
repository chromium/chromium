// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/chrome_utils/extension_file_logger.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"

namespace chrome_cleaner {
namespace {

void LogFilesInPath(const base::FilePath& path,
                    std::vector<internal::FileInformation>* files) {
  if (!base::PathExists(path))
    return;
  base::FileEnumerator file_enumerator(path, /*recursive=*/true,
                                       base::FileEnumerator::FileType::FILES);
  base::FilePath file_path = file_enumerator.Next();
  while (!file_path.empty()) {
    internal::FileInformation file_information;
    if (RetrieveFileInformation(file_path, /*include_details=*/true,
                                &file_information)) {
      files->push_back(file_information);
    }

    file_path = file_enumerator.Next();
  }
}

}  // namespace

ExtensionFileLogger::ExtensionFileLogger(const base::FilePath& user_data_path)
    : user_data_path_(user_data_path) {}

ExtensionFileLogger::~ExtensionFileLogger() = default;

bool ExtensionFileLogger::GetExtensionFiles(
    const std::wstring& extension_id,
    std::vector<internal::FileInformation>* files) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!initialized_)
    CacheProfilesAndExtensions();

  bool extension_found = false;

  for (auto it = installed_extensions_.begin();
       it != installed_extensions_.end(); it++) {
    if (it->second.find(extension_id) != it->second.end()) {
      extension_found = true;
      LogFilesInPath(it->first.Append(extension_id), files);
    }
  }

  return extension_found;
}

void ExtensionFileLogger::CacheExtensionIdsInProfile(
    const base::FilePath& profile_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::FilePath profile_extensions_path = profile_path.Append(L"Extensions");
  if (!base::PathExists(profile_extensions_path))
    return;

  base::FileEnumerator file_enumerator(
      profile_extensions_path, /*recursive=*/false,
      base::FileEnumerator::FileType::DIRECTORIES);

  base::FilePath extension_path = file_enumerator.Next();

  while (!extension_path.empty()) {
    installed_extensions_[profile_extensions_path].insert(
        extension_path.BaseName().value());
    extension_path = file_enumerator.Next();
  }
}

void ExtensionFileLogger::CacheProfilesAndExtensions() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  installed_extensions_.clear();

  // Check all the directories, if one is not a profile then
  // CacheExtensionIdsInProfile will return right away because there will
  // be no Extensions folder.
  base::FileEnumerator file_enumerator(
      user_data_path_, /*recursive=*/false,
      base::FileEnumerator::FileType::DIRECTORIES);
  base::FilePath profile_path = file_enumerator.Next();

  while (!profile_path.empty()) {
    CacheExtensionIdsInProfile(profile_path);
    profile_path = file_enumerator.Next();
  }
  initialized_ = true;
}

}  // namespace chrome_cleaner
