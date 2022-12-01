// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IMPORTER_FIREFOX_IMPORTER_UTILS_H_
#define CHROME_COMMON_IMPORTER_FIREFOX_IMPORTER_UTILS_H_

#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/values.h"
#include "build/build_config.h"

class GURL;

namespace base {
class FilePath;
}

#if BUILDFLAG(IS_WIN)
// Detects which version of Firefox is installed from registry. Returns its
// major version, and drops the minor version. Returns 0 if failed. If there are
// indicators of both Firefox 2 and Firefox 3 it is biased to return the biggest
// version.
int GetCurrentFirefoxMajorVersionFromRegistry();

// Detects where Firefox lives. Returns an empty path if Firefox is not
// installed.
base::FilePath GetFirefoxInstallPathFromRegistry();
#endif  // BUILDFLAG(IS_WIN)

struct FirefoxDetail {
  // |path| represents the Path field in Profiles.ini.
  // This path is the directory name where all the profile information
  // in stored.
  base::FilePath path;
  // The user specified name of the profile.
  std::u16string name;
};

inline bool operator==(const FirefoxDetail& a1, const FirefoxDetail& a2) {
  return a1.name == a2.name && a1.path == a2.path;
}

inline bool operator!=(const FirefoxDetail& a1, const FirefoxDetail& a2) {
  return !(a1 == a2);
}

// Returns a vector of FirefoxDetail for available profiles.
std::vector<FirefoxDetail> GetFirefoxDetails(
    const std::string& firefox_install_id);

// Returns the path to the Firefox profile, using a custom dictionary.
// If |firefox_install_id| is not empty returns the default profile associated
// with that id.
// Exposed for testing.
std::vector<FirefoxDetail> GetFirefoxDetailsFromDictionary(
    const base::Value::Dict& root,
    const std::string& firefox_install_id);

// Detects version of Firefox and installation path for the given Firefox
// profile.
bool GetFirefoxVersionAndPathFromProfile(const base::FilePath& profile_path,
                                         int* version,
                                         base::FilePath* app_path);

// Gets the full path of the profiles.ini file. This file records the profiles
// that can be used by Firefox. Returns an empty path if failed.
base::FilePath GetProfilesINI();

// Returns the home page set in Firefox in a particular profile.
GURL GetHomepage(const base::FilePath& profile_path);

// Checks to see if this home page is a default home page, as specified by
// the resource file browserconfig.properties in the Firefox application
// directory.
bool IsDefaultHomepage(const GURL& homepage, const base::FilePath& app_path);

// Parses the value of a particular firefox preference from a string that is the
// contents of the prefs file.
std::string GetPrefsJsValue(const std::string& prefs,
                            const std::string& pref_key);

// Returns the localized Firefox branding name.
// This is useful to differentiate between Firefox and Iceweasel.
// If anything goes wrong while trying to obtain the branding name,
// the function assumes it's Firefox.
std::u16string GetFirefoxImporterName(const base::FilePath& app_path);

#endif  // CHROME_COMMON_IMPORTER_FIREFOX_IMPORTER_UTILS_H_
