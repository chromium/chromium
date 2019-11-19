// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/installed_applications.h"

#include <algorithm>

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "chrome/browser/win/conflicts/msi_util.h"

namespace {

// Returns true if |candidate| is registered as a system component.
bool IsSystemComponent(const base::win::RegKey& candidate) {
  DWORD system_component = 0;
  return candidate.ReadValueDW(L"SystemComponent", &system_component) ==
             ERROR_SUCCESS &&
         system_component == 1;
}

// Fetches a string |value| out of |key|. Return false if a non-empty value
// could not be retrieved.
bool GetValue(const base::win::RegKey& key,
              const wchar_t* value,
              base::string16* result) {
  return key.ReadValue(value, result) == ERROR_SUCCESS && !result->empty();
}

// Try to get the |install_path| from |candidate| using the InstallLocation
// value. Return true on success.
bool GetInstallPathUsingInstallLocation(const base::win::RegKey& candidate,
                                        base::FilePath* install_path) {
  base::string16 install_location;
  if (GetValue(candidate, L"InstallLocation", &install_location)) {
    *install_path = base::FilePath(std::move(install_location));
    return true;
  }
  return false;
}

// Returns true if the |component_path| points to a registry key. Registry key
// paths are characterized by a number instead of a drive letter.
// See the documentation for ::MsiGetComponentPath():
// https://msdn.microsoft.com/library/windows/desktop/aa370112.aspx
bool IsRegistryComponentPath(const base::string16& component_path) {
  base::string16 drive_letter =
      component_path.substr(0, component_path.find(':'));

  for (const wchar_t* registry_drive_letter :
       {L"00", L"01", L"02", L"03", L"20", L"21", L"22", L"23"}) {
    if (drive_letter == registry_drive_letter)
      return true;
  }

  return false;
}

// Returns all the files installed by the product identified by |product_guid|.
// Returns true on success.
bool GetInstalledFilesUsingMsiGuid(
    const base::string16& product_guid,
    const MsiUtil& msi_util,
    const base::string16& user_sid,
    std::vector<base::FilePath>* installed_files) {
  // An invalid product guid may have been passed to this function. In this
  // case, GetMsiComponentPaths() will return false so it is not necessary to
  // specifically filter those out.
  std::vector<base::string16> component_paths;
  if (!msi_util.GetMsiComponentPaths(product_guid, user_sid, &component_paths))
    return false;

  for (auto& component_path : component_paths) {
    // Exclude registry component paths.
    if (IsRegistryComponentPath(component_path))
      continue;

    installed_files->push_back(base::FilePath(std::move(component_path)));
  }

  return true;
}

// Helper function to sort |container| using CompareLessIgnoreCase.
void SortByFilePaths(
    std::vector<std::pair<base::FilePath, size_t>>* container) {
  std::sort(container->begin(), container->end(),
            [](const auto& lhs, const auto& rhs) {
              return base::FilePath::CompareLessIgnoreCase(lhs.first.value(),
                                                           rhs.first.value());
            });
}

}  // namespace

InstalledApplications::InstalledApplications()
    : InstalledApplications(std::make_unique<MsiUtil>()) {}

InstalledApplications::~InstalledApplications() = default;

bool InstalledApplications::GetInstalledApplications(
    const base::FilePath& file,
    std::vector<ApplicationInfo>* applications) const {
  // First, check if an exact file match exists in the installed files list.
  if (GetApplicationsFromInstalledFiles(file, applications))
    return true;

  // Then try to find a parent directory in the install directories list.
  return GetApplicationsFromInstallDirectories(file, applications);
}

InstalledApplications::InstalledApplications(
    std::unique_ptr<MsiUtil> msi_util) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  SCOPED_UMA_HISTOGRAM_TIMER(
      "ThirdPartyModules.InstalledApplications.GetDataTime");

  // Iterate over all the variants of the uninstall registry key.
  static constexpr wchar_t kUninstallKeyPath[] =
      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall";

  std::vector<std::pair<HKEY, REGSAM>> registry_key_combinations;
  if (base::win::OSInfo::GetArchitecture() ==
      base::win::OSInfo::X86_ARCHITECTURE) {
    // On 32-bit Windows, there is only one view of the registry.
    registry_key_combinations.emplace_back(HKEY_CURRENT_USER, 0);
    registry_key_combinations.emplace_back(HKEY_LOCAL_MACHINE, 0);
  } else {
    // On 64-bit Windows, there also exists a 32-bit view (Wow6432Node). Except
    // that the "HKCU\SOFTWARE\" subtree is shared between the 32-bits and
    // 64 bits views. Accessing both would create duplicate entries.
    // See https://msdn.microsoft.com/library/windows/desktop/aa384253.aspx
    registry_key_combinations.emplace_back(HKEY_CURRENT_USER, 0);
    registry_key_combinations.emplace_back(HKEY_LOCAL_MACHINE, KEY_WOW64_32KEY);
    registry_key_combinations.emplace_back(HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY);
  }

  // Retrieve the current user's Security Identifier. If it fails, |user_sid|
  // will stay empty.
  base::string16 user_sid;
  bool got_user_sid_string = base::win::GetUserSidString(&user_sid);
  UMA_HISTOGRAM_BOOLEAN(
      "ThirdPartyModules.InstalledApplications.GotUserSidString",
      got_user_sid_string);

  for (const auto& combination : registry_key_combinations) {
    for (base::win::RegistryKeyIterator i(combination.first, kUninstallKeyPath,
                                          combination.second);
         i.Valid(); ++i) {
      CheckRegistryKeyForInstalledApplication(
          combination.first, kUninstallKeyPath, combination.second, i.Name(),
          *msi_util, user_sid);
    }
  }

  // The vectors are sorted so that binary searching can be used. No additional
  // entries will be added anyways.
  SortByFilePaths(&installed_files_);
  SortByFilePaths(&install_directories_);
}

void InstalledApplications::CheckRegistryKeyForInstalledApplication(
    HKEY hkey,
    const base::string16& key_path,
    REGSAM wow64access,
    const base::string16& key_name,
    const MsiUtil& msi_util,
    const base::string16& user_sid) {
  base::string16 candidate_key_path =
      base::StringPrintf(L"%ls\\%ls", key_path.c_str(), key_name.c_str());
  base::win::RegKey candidate(hkey, candidate_key_path.c_str(),
                              KEY_QUERY_VALUE | wow64access);

  if (!candidate.Valid())
    return;

  // System components are not displayed in the Add or remove applications list.
  if (IsSystemComponent(candidate))
    return;

  // If there is no UninstallString, the Uninstall button is grayed out.
  base::string16 uninstall_string;
  if (!GetValue(candidate, L"UninstallString", &uninstall_string))
    return;

  // Ignore Microsoft applications.
  base::string16 publisher;
  if (GetValue(candidate, L"Publisher", &publisher) &&
      base::StartsWith(publisher, L"Microsoft", base::CompareCase::SENSITIVE)) {
    return;
  }

  // Because this class is used to display a warning to the user, not having
  // a display name renders the warning somewhat useless. Ignore those
  // candidates.
  base::string16 display_name;
  if (!GetValue(candidate, L"DisplayName", &display_name))
    return;

  base::FilePath install_path;
  if (GetInstallPathUsingInstallLocation(candidate, &install_path)) {
    applications_.push_back({std::move(display_name), hkey,
                             std::move(candidate_key_path), wow64access});

    const size_t application_index = applications_.size() - 1;
    install_directories_.emplace_back(std::move(install_path),
                                      application_index);
    return;
  }

  std::vector<base::FilePath> installed_files;
  if (GetInstalledFilesUsingMsiGuid(key_name, msi_util, user_sid,
                                    &installed_files)) {
    applications_.push_back({std::move(display_name), hkey,
                             std::move(candidate_key_path), wow64access});

    const size_t application_index = applications_.size() - 1;
    for (auto& installed_file : installed_files) {
      installed_files_.emplace_back(std::move(installed_file),
                                    application_index);
    }
  }
}

bool InstalledApplications::GetApplicationsFromInstalledFiles(
    const base::FilePath& file,
    std::vector<ApplicationInfo>* applications) const {
  // This functor is used to find all exact items by their key in a collection
  // of key/value pairs.
  struct FilePathLess {
    bool operator()(const std::pair<base::FilePath, size_t> element,
                    const base::FilePath& file) {
      return base::FilePath::CompareLessIgnoreCase(element.first.value(),
                                                   file.value());
    }
    bool operator()(const base::FilePath& file,
                    const std::pair<base::FilePath, size_t> element) {
      return base::FilePath::CompareLessIgnoreCase(file.value(),
                                                   element.first.value());
    }
  };

  auto equal_range = std::equal_range(
      installed_files_.begin(), installed_files_.end(), file, FilePathLess());

  auto nb_matches = std::distance(equal_range.first, equal_range.second);
  if (nb_matches == 0)
    return false;

  applications->reserve(applications->size() + nb_matches);
  for (auto iter = equal_range.first; iter != equal_range.second; ++iter)
    applications->push_back(applications_[iter->second]);

  return true;
}

bool InstalledApplications::GetApplicationsFromInstallDirectories(
    const base::FilePath& file,
    std::vector<ApplicationInfo>* applications) const {
  // This functor is used to find all matching items by their key in a
  // collection of key/value pairs. This also takes advantage of the fact that
  // only the first element of the pair is a directory.
  struct FilePathParentLess {
    bool operator()(const std::pair<base::FilePath, size_t> directory,
                    const base::FilePath& file) {
      if (directory.first.IsParent(file))
        return false;
      return base::FilePath::CompareLessIgnoreCase(directory.first.value(),
                                                   file.value());
    }
    bool operator()(const base::FilePath& file,
                    const std::pair<base::FilePath, size_t> directory) {
      if (directory.first.IsParent(file))
        return false;
      return base::FilePath::CompareLessIgnoreCase(file.value(),
                                                   directory.first.value());
    }
  };

  auto equal_range =
      std::equal_range(install_directories_.begin(), install_directories_.end(),
                       file, FilePathParentLess());

  // Skip cases where there are multiple matches because there is no way to know
  // which application is the real owner of the |file| with the information this
  // class possess.
  if (std::distance(equal_range.first, equal_range.second) != 1)
    return false;

  applications->push_back(applications_[equal_range.first->second]);
  return true;
}

bool operator<(const InstalledApplications::ApplicationInfo& lhs,
               const InstalledApplications::ApplicationInfo& rhs) {
  return std::tie(lhs.name, lhs.registry_root, lhs.registry_key_path,
                  lhs.registry_wow64_access) <
         std::tie(rhs.name, rhs.registry_root, rhs.registry_key_path,
                  rhs.registry_wow64_access);
}
