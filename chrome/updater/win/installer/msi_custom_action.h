// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_INSTALLER_MSI_CUSTOM_ACTION_H_
#define CHROME_UPDATER_WIN_INSTALLER_MSI_CUSTOM_ACTION_H_

#include <windows.h>

#include <msi.h>

#include <string>
#include <vector>

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

// Interface wrapper around MSI API calls to allow for mocking.
class MsiHandleInterface {
 public:
  virtual ~MsiHandleInterface() = default;

  // Thin wrapper around `::MsiGetProperty`. Gets the value of the `name`
  // property in `value`/`value_length`, as per the `::MsiGetProperty` API.
  // `value_length` is redundant for a vector, but this is meant to be a thin
  // wrapper to ensure maximum code coverage.
  virtual UINT GetProperty(const std::wstring& name,
                           std::vector<wchar_t>& value,
                           DWORD& value_length) const = 0;

  // Thin wrapper around `::MsiSetProperty`. Sets the value of the `name`
  // property with `value`.
  virtual UINT SetProperty(const std::string& name,
                           const std::string& value) = 0;
};

// Reads the tagged information from the provided `msi_handle`, and sets MSI
// properties on `msi_handle`.
UINT MsiSetTags(MsiHandleInterface& msi_handle);

}  // namespace updater

// A DLL custom action entrypoint wrapper for `MsiSetTags`.
extern "C" UINT __stdcall ExtractTagInfoFromInstaller(MSIHANDLE msi_handle);

#endif  // CHROME_UPDATER_WIN_INSTALLER_MSI_CUSTOM_ACTION_H_
