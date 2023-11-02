// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_CHROME_UTILS_EXTENSION_FILE_LOGGER_H_
#define CHROME_CHROME_CLEANER_CHROME_UTILS_EXTENSION_FILE_LOGGER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "chrome/chrome_cleaner/os/disk_util.h"

namespace chrome_cleaner {

typedef std::map<base::FilePath, std::set<std::wstring>>
    ExtensionsInProfilesMap;

// Utility class to extract the file information of all the files of a given
// extension.
//
// Please note that this class may have negative performance implications if
// used in more than one place, please avoid creating multiple instances of it.
class ExtensionFileLogger {
 public:
  explicit ExtensionFileLogger(const base::FilePath& user_data_path);
  ~ExtensionFileLogger();

  // Searches through all of the cached user's profiles for an extension with
  // id |extension_id|. If one found, fills |files| with data about the
  // extension's files.
  // Returns false if it cannot find the extension folder.
  //
  // If two profiles have the same extension installed, both extension's files
  // are going to be logged. This is intended since we are not sure if there
  // are modified files in one of the extensions.
  bool GetExtensionFiles(const std::wstring& extension_id,
                         std::vector<internal::FileInformation>* files);

 private:
  void CacheProfilesAndExtensions();
  void CacheExtensionIdsInProfile(const base::FilePath& profile_path);

  ExtensionsInProfilesMap installed_extensions_;
  base::FilePath user_data_path_;
  bool initialized_ = false;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_CHROME_UTILS_EXTENSION_FILE_LOGGER_H_
