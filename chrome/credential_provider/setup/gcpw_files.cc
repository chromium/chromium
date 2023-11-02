// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/setup/gcpw_files.h"

#include "chrome/credential_provider/extension/extension_utils.h"

namespace credential_provider {

base::FilePath::StringType kCredentialProviderDll =
    FILE_PATH_LITERAL("Gaia1_0.dll");

base::FilePath::StringType kCredentialProviderSetupExe =
    FILE_PATH_LITERAL("gcp_setup.exe");

base::FilePath::StringType kEventLogProviderDll =
    FILE_PATH_LITERAL("gcp_eventlog_provider.dll");

base::FilePath::StringType kCredentialProviderExtensionExe =
    FILE_PATH_LITERAL("extension\\gcpw_extension.exe");

// List of files to install.  If the file list is changed here, make sure to
// update the files added in make_setup.py.
const std::vector<base::FilePath::StringType> kFileNames = {
    kCredentialProviderSetupExe, kEventLogProviderDll,
    kCredentialProviderExtensionExe,
    kCredentialProviderDll,  // Base name to the CP dll.
};

// List of dlls to register.  Must be a subset of kFilenames.
const std::vector<base::FilePath::StringType> kRegsiterDlls = {
    kCredentialProviderDll,
};

GCPWFiles::~GCPWFiles() {}

std::vector<base::FilePath::StringType> GCPWFiles::GetEffectiveInstallFiles() {
  std::vector<base::FilePath::StringType> files;
  for (auto& file : kFileNames) {
    if (file.compare(kCredentialProviderExtensionExe) == 0 &&
        !extension::IsGCPWExtensionEnabled())
      continue;
    files.push_back(file);
  }

  return files;
}

std::vector<base::FilePath::StringType> GCPWFiles::GetRegistrationFiles() {
  return kRegsiterDlls;
}

// static
GCPWFiles** GCPWFiles::GetInstanceStorage() {
  static GCPWFiles* instance = new GCPWFiles();
  return &instance;
}

// static
GCPWFiles* GCPWFiles::Get() {
  return *GetInstanceStorage();
}

}  // namespace credential_provider
