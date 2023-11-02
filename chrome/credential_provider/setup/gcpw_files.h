// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_SETUP_GCPW_FILES_H_
#define CHROME_CREDENTIAL_PROVIDER_SETUP_GCPW_FILES_H_

#include <vector>

#include "base/files/file_path.h"

namespace credential_provider {

extern base::FilePath::StringType kCredentialProviderSetupExe;
extern base::FilePath::StringType kCredentialProviderExtensionExe;

// Provides a common way of retrieving list of files to copy from 7zip archive
// into the system folder. Anyone who wants to iterate over these files, need to
// use GetEffectiveInstallFiles function which may filter some of the files
// based on the registry values.
class GCPWFiles {
 public:
  static GCPWFiles* Get();

  virtual ~GCPWFiles();

  // Returns the effective list of installable files from 7zip archive file
  // structure into system folder.
  virtual std::vector<base::FilePath::StringType> GetEffectiveInstallFiles();

  // List of COM DLLs to register.
  virtual std::vector<base::FilePath::StringType> GetRegistrationFiles();

 protected:
  GCPWFiles() {}

  // Returns the storage used for the instance pointer.
  static GCPWFiles** GetInstanceStorage();
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_SETUP_GCPW_FILES_H_
