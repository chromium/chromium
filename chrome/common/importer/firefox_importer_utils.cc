// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/importer/firefox_importer_utils.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <string>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/ini_parser.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

// Retrieves the file system path of the profile name.
base::FilePath GetProfilePath(const base::DictionaryValue& root,
                              const std::string& profile_name) {
  base::string16 path16;
  std::string is_relative;
  if (!root.GetStringASCII(profile_name + ".IsRelative", &is_relative) ||
      !root.GetString(profile_name + ".Path", &path16))
    return base::FilePath();

#if defined(OS_WIN)
  base::ReplaceSubstringsAfterOffset(
      &path16, 0, base::ASCIIToUTF16("/"), base::ASCIIToUTF16("\\"));
#endif
  base::FilePath path = base::FilePath::FromUTF16Unsafe(path16);

  // IsRelative=1 means the folder path would be relative to the
  // path of profiles.ini. IsRelative=0 refers to a custom profile
  // location.
  if (is_relative == "1")
    path = GetProfilesINI().DirName().Append(path);

  return path;
}

// Returns a map from Firefox profiles to their corresponding installation ids.
// The keys are file system paths for Firefox profiles that are the default
// profile in their installation. The values are the registry keys for the
// corresponding installation.
std::map<std::string, std::string> GetDefaultProfilesPerInstall(
    const base::DictionaryValue& root) {
  std::map<std::string, std::string> default_profile_to_install_id;
  static constexpr base::StringPiece kInstallPrefix("Install");
  // Find the default profiles for each Firefox installation.
  for (const auto& data : root) {
    const std::string& dict_key = data.first;
    if (base::StartsWith(dict_key, kInstallPrefix,
                         base::CompareCase::SENSITIVE)) {
      std::string path;
      if (root.GetStringASCII(dict_key + ".Default", &path)) {
        default_profile_to_install_id.emplace(
            std::move(path), dict_key.substr(kInstallPrefix.size()));
      }
    }
  }
  return default_profile_to_install_id;
}

base::FilePath GetLegacyDefaultProfilePath(
    const base::DictionaryValue& root,
    const std::vector<std::string>& profile_names) {
  if (profile_names.empty())
    return base::FilePath();

  // When multiple profiles exist, the path to the default profile is returned.
  for (const auto& profile_name : profile_names) {
    // Checks if the named profile is the default profile using the legacy
    // format of profiles.ini (Firefox version < 67).
    std::string is_default;
    if (root.GetStringASCII(profile_name + ".Default", &is_default) &&
        is_default == "1") {
      return GetProfilePath(root, profile_name);
    }
  }

  // If no default profile is found, the path to Profile0 will be returned.
  return GetProfilePath(root, profile_names.front());
}

} // namespace

base::FilePath GetFirefoxProfilePath(const std::string& firefox_install_id) {
  base::FilePath ini_file = GetProfilesINI();
  std::string content;
  base::ReadFileToString(ini_file, &content);
  DictionaryValueINIParser ini_parser;
  ini_parser.Parse(content);
  return GetFirefoxProfilePathFromDictionary(ini_parser.root(),
                                             firefox_install_id);
}

base::FilePath GetFirefoxProfilePathFromDictionary(
    const base::DictionaryValue& root,
    const std::string& firefox_install_id) {
  // List of profiles linked to a Firefox installation. This will be empty for
  // Firefox versions older than 67.
  std::map<std::string, std::string> default_profile_to_install_id =
      GetDefaultProfilesPerInstall(root);
  // First profile linked to a Firefox installation (version >= 67).
  base::Optional<std::string> first_modern_profile;

  // Profiles not linked to a Firefox installation (version < 67).
  std::vector<std::string> legacy_profiles;

  for (int i = 0; ; ++i) {
    std::string current_profile = base::StringPrintf("Profile%d", i);
    if (!root.HasKey(current_profile)) {
      // Profiles are contiguously numbered. So we exit when we can't
      // find the i-th one.
      break;
    }

    std::string path;
    if (!root.GetStringASCII(current_profile + ".Path", &path))
      continue;

    auto install_id_it = default_profile_to_install_id.find(path);
    if (install_id_it != default_profile_to_install_id.end()) {
      // If this installation is the default browser, use the associated
      // profile as default profile.
      if (install_id_it->second == firefox_install_id)
        return GetProfilePath(root, current_profile);
      if (!first_modern_profile)
        first_modern_profile.emplace(std::move(current_profile));
    } else {
      // If no Firefox installation found in profiles.ini, legacy profiles
      // (Firefox version < 67) are being used.
      legacy_profiles.push_back(std::move(current_profile));
    }
  }

  // Take the first install found as the default install.
  if (first_modern_profile)
    return GetProfilePath(root, *first_modern_profile);

  return GetLegacyDefaultProfilePath(root, legacy_profiles);
}

#if defined(OS_MACOSX)
// Find the "*.app" component of the path and build up from there.
// The resulting path will be .../Firefox.app/Contents/MacOS.
// We do this because we don't trust LastAppDir to always be
// this particular path, without any subdirs, and we want to make
// our assumption about Firefox's root being in that path explicit.
bool ComposeMacAppPath(const std::string& path_from_file,
                       base::FilePath* output) {
  base::FilePath path(path_from_file);
  typedef std::vector<base::FilePath::StringType> ComponentVector;
  ComponentVector path_components;
  path.GetComponents(&path_components);
  if (path_components.empty())
    return false;
  // The first path component is special because it may be absolute. Calling
  // Append with an absolute path component will trigger an assert, so we
  // must handle it differently and initialize output with it.
  *output = base::FilePath(path_components[0]);
  // Append next path components untill we find the *.app component. When we do,
  // append Contents/MacOS.
  for (size_t i = 1; i < path_components.size(); ++i) {
    *output = output->Append(path_components[i]);
    if (base::EndsWith(path_components[i], ".app",
                       base::CompareCase::SENSITIVE)) {
      *output = output->Append("Contents");
      *output = output->Append("MacOS");
      return true;
    }
  }
  LOG(ERROR) << path_from_file << " doesn't look like a valid Firefox "
             << "installation path: missing /*.app/ directory.";
  return false;
}
#endif  // OS_MACOSX

bool GetFirefoxVersionAndPathFromProfile(const base::FilePath& profile_path,
                                         int* version,
                                         base::FilePath* app_path) {
  bool ret = false;
  base::FilePath compatibility_file =
      profile_path.AppendASCII("compatibility.ini");
  std::string content;
  base::ReadFileToString(compatibility_file, &content);
  base::ReplaceSubstringsAfterOffset(&content, 0, "\r\n", "\n");

  for (const std::string& line : base::SplitString(
           content, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    if (line.empty() || line[0] == '#' || line[0] == ';')
      continue;
    size_t equal = line.find('=');
    if (equal != std::string::npos) {
      std::string key = line.substr(0, equal);
      if (key == "LastVersion") {
        base::StringToInt(line.substr(equal + 1), version);
        ret = true;
      } else if (key == "LastAppDir") {
        // TODO(evanm): If the path in question isn't convertible to
        // UTF-8, what does Firefox do?  If it puts raw bytes in the
        // file, we could go straight from bytes -> filepath;
        // otherwise, we're out of luck here.
#if defined(OS_MACOSX)
        // Extract path from "LastAppDir=/actual/path"
        size_t separator_pos = line.find_first_of('=');
        const std::string& path_from_ini = line.substr(separator_pos + 1);
        if (!ComposeMacAppPath(path_from_ini, app_path))
          return false;
#else  // !OS_MACOSX
        *app_path = base::FilePath::FromUTF8Unsafe(line.substr(equal + 1));
#endif  // OS_MACOSX
      }
    }
  }
  return ret;
}

bool ReadPrefFile(const base::FilePath& path, std::string* content) {
  if (content == NULL)
    return false;

  base::ReadFileToString(path, content);

  if (content->empty()) {
    LOG(WARNING) << "Firefox preference file " << path.value() << " is empty.";
    return false;
  }

  return true;
}

std::string ReadBrowserConfigProp(const base::FilePath& app_path,
                                  const std::string& pref_key) {
  std::string content;
  if (!ReadPrefFile(app_path.AppendASCII("browserconfig.properties"), &content))
    return std::string();

  // This file has the syntax: key=value.
  size_t prop_index = content.find(pref_key + "=");
  if (prop_index == std::string::npos)
    return std::string();

  size_t start = prop_index + pref_key.length();
  size_t stop = std::string::npos;
  if (start != std::string::npos)
    stop = content.find("\n", start + 1);

  if (start == std::string::npos ||
      stop == std::string::npos || (start == stop)) {
    LOG(WARNING) << "Firefox property " << pref_key << " could not be parsed.";
    return std::string();
  }

  return content.substr(start + 1, stop - start - 1);
}

std::string ReadPrefsJsValue(const base::FilePath& profile_path,
                             const std::string& pref_key) {
  std::string content;
  if (!ReadPrefFile(profile_path.AppendASCII("prefs.js"), &content))
    return std::string();

  return GetPrefsJsValue(content, pref_key);
}

GURL GetHomepage(const base::FilePath& profile_path) {
  std::string home_page_list =
      ReadPrefsJsValue(profile_path, "browser.startup.homepage");

  size_t seperator = home_page_list.find_first_of('|');
  if (seperator == std::string::npos)
    return GURL(home_page_list);

  return GURL(home_page_list.substr(0, seperator));
}

bool IsDefaultHomepage(const GURL& homepage, const base::FilePath& app_path) {
  if (!homepage.is_valid())
    return false;

  std::string default_homepages =
      ReadBrowserConfigProp(app_path, "browser.startup.homepage");

  size_t seperator = default_homepages.find_first_of('|');
  if (seperator == std::string::npos)
    return homepage.spec() == GURL(default_homepages).spec();

  // Crack the string into separate homepage urls.
  for (const std::string& url : base::SplitString(
           default_homepages, "|",
           base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    if (homepage.spec() == GURL(url).spec())
      return true;
  }

  return false;
}

std::string GetPrefsJsValue(const std::string& content,
                            const std::string& pref_key) {
  // This file has the syntax: user_pref("key", value);
  std::string search_for = std::string("user_pref(\"") + pref_key +
                           std::string("\", ");
  size_t prop_index = content.find(search_for);
  if (prop_index == std::string::npos)
    return std::string();

  size_t start = prop_index + search_for.length();
  size_t stop = std::string::npos;
  if (start != std::string::npos) {
    // Stop at the last ')' on this line.
    stop = content.find("\n", start + 1);
    stop = content.rfind(")", stop);
  }

  if (start == std::string::npos || stop == std::string::npos ||
      stop < start) {
    LOG(WARNING) << "Firefox property " << pref_key << " could not be parsed.";
    return std::string();
  }

  // String values have double quotes we don't need to return to the caller.
  if (content[start] == '\"' && content[stop - 1] == '\"') {
    ++start;
    --stop;
  }

  return content.substr(start, stop - start);
}

// The branding name is obtained from the application.ini file from the Firefox
// application directory. A sample application.ini file is the following:
//   [App]
//   Vendor=Mozilla
//   Name=Iceweasel
//   Profile=mozilla/firefox
//   Version=3.5.16
//   BuildID=20120421070307
//   Copyright=Copyright (c) 1998 - 2010 mozilla.org
//   ID={ec8030f7-c20a-464f-9b0e-13a3a9e97384}
//   .........................................
// In this example the function returns "Iceweasel" (or a localized equivalent).
base::string16 GetFirefoxImporterName(const base::FilePath& app_path) {
  const base::FilePath app_ini_file = app_path.AppendASCII("application.ini");
  std::string branding_name;
  if (base::PathExists(app_ini_file)) {
    std::string content;
    base::ReadFileToString(app_ini_file, &content);

    const std::string name_attr("Name=");
    bool in_app_section = false;
    for (const base::StringPiece& line : base::SplitStringPiece(
             content, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
      if (line == "[App]") {
        in_app_section = true;
      } else if (in_app_section) {
        if (base::StartsWith(line, name_attr, base::CompareCase::SENSITIVE)) {
          line.substr(name_attr.size()).CopyToString(&branding_name);
          break;
        }
        if (line.length() > 0 && line[0] == '[') {
          // No longer in the [App] section.
          break;
        }
      }
    }
  }

  branding_name = base::ToLowerASCII(branding_name);
  if (branding_name.find("iceweasel") != std::string::npos)
    return l10n_util::GetStringUTF16(IDS_IMPORT_FROM_ICEWEASEL);
  return l10n_util::GetStringUTF16(IDS_IMPORT_FROM_FIREFOX);
}
