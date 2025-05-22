// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_CONFLICTS_INSTALLED_APPLICATIONS_H_
#define CHROME_BROWSER_WIN_CONFLICTS_INSTALLED_APPLICATIONS_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/win/registry.h"
#include "base/win/windows_types.h"

class MsiUtil;

// This class inspects the user's installed applications and builds a mapping of
// files to its associated application.
//
// Installed applications are found by searching the
// "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall" registry key and
// its variants. There are 2 cases that are covered:
//
// 1 - If the application's installer did its due dilligence, it populated the
//     "InstallLocation" registry key with the directory where it was installed,
//     and all the files under that directory are assumed to be owned by this
//     application.
//
//     In the event of 2 conflicting "InstallLocation", both are ignored as this
//     method doesn't let us know for sure who is the owner of any enclosing
//     files.
//
// 2 - If the application's entry is a valid MSI Product GUID, the complete list
// of
//     associated file is used to exactly match a given file to a application.
//
//     If multiple products installed the same file as the same component,
//     Windows keeps a reference count of that component so that the file
//     doesn't get removed if one of them is uninstalled. So both applications
//     are returned by GetInstalledApplications().
//
//  Note: Applications may be skipped and so would not be returned by
//        GetInstalledApplications() for the following reasons:
//        - The application is owned by Microsoft.
//        - The uninstall entry is marked as a system component.
//        - The uninstall entry has no display name.
//        - The uninstall entry has no UninstallString.
//
class InstalledApplications {
 public:
  struct ApplicationInfo {
    std::wstring name;

    // Holds the path to the uninstall entry in the registry.
    HKEY registry_root;
    std::wstring registry_key_path;
    REGSAM registry_wow64_access;
  };

  // Initializes this instance with the list of installed applications. While
  // the constructor must be called in a sequence that allows blocking, its
  // public method can be used without such restrictions.
  InstalledApplications();

  InstalledApplications(const InstalledApplications&) = delete;
  InstalledApplications& operator=(const InstalledApplications&) = delete;

  virtual ~InstalledApplications();

  virtual std::vector<std::pair<HKEY, REGSAM>> GenRegistryKeyCombinations()
      const;

  // Given a |file|, checks if it matches an installed application on the user's
  // machine and appends all the matching applications to |applications|.
  // Virtual to allow mocking.
  virtual bool GetInstalledApplications(
      const base::FilePath& file,
      std::vector<ApplicationInfo>* applications) const;

 protected:
  // Protected so that tests can subclass InstalledApplications and access it.
  explicit InstalledApplications(std::unique_ptr<MsiUtil> msi_util);

 private:
  FRIEND_TEST_ALL_PREFIXES(InstalledApplicationsTest, NoDuplicates);

  // If the registry key references a valid installed application, this function
  // adds an entry to |applications_| with its list of files or installation
  // directory to their associated vector.
  void CheckRegistryKeyForInstalledApplication(HKEY hkey,
                                               const std::wstring& key_path,
                                               REGSAM wow64access,
                                               const std::wstring& key_name,
                                               const MsiUtil& msi_util,
                                               const std::wstring& user_sid);

  bool GetApplicationsFromInstalledFiles(
      const base::FilePath& file,
      std::vector<ApplicationInfo>* applications) const;
  bool GetApplicationsFromInstallDirectories(
      const base::FilePath& file,
      std::vector<ApplicationInfo>* applications) const;

  // Applications are stored in this vector because multiple entries in
  // |installed_files| could point to the same one. This is to avoid
  // duplicating them.
  std::vector<ApplicationInfo> applications_;

  // Contains all the files from applications installed via Microsoft Installer.
  // The second part of the pair is the index into |applications|.
  std::vector<std::pair<base::FilePath, size_t>> installed_files_;

  // For some applications, the best information available is the directory of
  // the installation. The compare functor treats file paths where one is the
  // parent of the other as equal.
  // The second part of the pair is the index into |applications|.
  std::vector<std::pair<base::FilePath, size_t>> install_directories_;
};

bool operator<(const InstalledApplications::ApplicationInfo& lhs,
               const InstalledApplications::ApplicationInfo& rhs);

#endif  // CHROME_BROWSER_WIN_CONFLICTS_INSTALLED_APPLICATIONS_H_
