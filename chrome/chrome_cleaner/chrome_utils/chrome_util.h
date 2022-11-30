// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_CHROME_UTILS_CHROME_UTIL_H_
#define CHROME_CHROME_CLEANER_CHROME_UTILS_CHROME_UTIL_H_

#include <set>
#include <string>

namespace base {
class FilePath;
}  // namespace base

namespace chrome_cleaner {

// Chrome shortcut filename.
extern const wchar_t kChromeShortcutFilename[];
// The KO language version doesn't have the term Google in the filename.
extern const wchar_t kKOChromeShortcutFilename[];

// Retrieve installed chrome version to |chrome_version|. The flag
// |system_install| receives whether the chrome is installed system wide or per
// user. |system_install| is optional and can be null.
// Return true on success.
bool RetrieveChromeVersionAndInstalledDomain(std::wstring* chrome_version,
                                             bool* system_install);

// Retrieve path to Chrome's executable from the path given on the command
// line. Return true if Chrome's exe path was given on the command line and the
// path exists.
bool RetrieveChromeExePathFromCommandLine(base::FilePath* chrome_exe_path);

// Search for all Chrome executable path directories, for example
// "C:\Program Files\Google\Chrome\Application".
void ListChromeExeDirectories(std::set<base::FilePath>* paths);

// Search for all Chrome versioned installation paths, for example
// "C:\Program Files\Google\Chrome\Application\68.0.3440.84".
void ListChromeInstallationPaths(std::set<base::FilePath>* paths);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_CHROME_UTILS_CHROME_UTIL_H_
