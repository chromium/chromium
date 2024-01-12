// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_INSTALLER_MSI_CUSTOM_ACTION_H_
#define CHROME_UPDATER_WIN_INSTALLER_MSI_CUSTOM_ACTION_H_

#include <windows.h>

#include <msi.h>

#include <string>
#include <vector>

namespace updater {

// Interface wrapper around MSI API calls to allow for mocking.
class MsiHandleInterface {
 public:
  virtual ~MsiHandleInterface() = default;

  // Gets the value of the `name` property in `value`/`value_length`.
  virtual UINT GetProperty(const std::wstring& name,
                           std::vector<wchar_t>& value,
                           DWORD& value_length) const = 0;

  // Sets the value of the `name` property with `value`.
  virtual UINT SetProperty(const std::string& name,
                           const std::string& value) = 0;

  // Creates a new record object with the requested `field_count`.
  virtual MSIHANDLE CreateRecord(UINT field_count) = 0;

  // Copies `value` into the designated `field_index` of the record object
  // `record_handle`.
  virtual UINT RecordSetString(MSIHANDLE record_handle,
                               UINT field_index,
                               const std::wstring& value) = 0;

  // Sends an error record object `record_handle` with the given `message_type`
  // to the installer for processing.
  virtual int ProcessMessage(INSTALLMESSAGE message_type,
                             MSIHANDLE record_handle) = 0;
};

// Reads the tagged information from the provided `msi_handle`, and sets MSI
// properties on `msi_handle`.
UINT MsiSetTags(MsiHandleInterface& msi_handle);

// Reads the last installer result from the registry, and sets the string value
// on `msi_handle`.
UINT MsiSetInstallerResult(MsiHandleInterface& msi_handle);
}  // namespace updater

// A DLL custom action entrypoint wrapper for `MsiSetTags`.
extern "C" UINT __stdcall ExtractTagInfoFromInstaller(MSIHANDLE msi_handle);

// A DLL custom action entrypoint wrapper for `MsiSetInstallerResult`.
extern "C" UINT __stdcall ShowInstallerResultUIString(MSIHANDLE msi_handle);

#endif  // CHROME_UPDATER_WIN_INSTALLER_MSI_CUSTOM_ACTION_H_
