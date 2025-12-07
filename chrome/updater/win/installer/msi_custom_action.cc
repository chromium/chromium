// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/installer/msi_custom_action.h"

#include <windows.h>

#include <msi.h>
#include <msiquery.h>

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/win/registry.h"
#include "chrome/updater/tag.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/win_constants.h"

namespace updater {

namespace {

class MsiHandleImpl : public MsiHandleInterface {
 public:
  explicit MsiHandleImpl(MSIHANDLE msi_handle);
  ~MsiHandleImpl() override;
  UINT GetProperty(const std::wstring& name,
                   std::vector<wchar_t>& value,
                   DWORD& value_length) const override;
  UINT SetProperty(const std::string& name, const std::string& value) override;
  MSIHANDLE CreateRecord(UINT field_count) override;
  UINT RecordSetString(MSIHANDLE record_handle,
                       UINT field_index,
                       const std::wstring& value) override;
  int ProcessMessage(INSTALLMESSAGE message_type,
                     MSIHANDLE record_handle) override;

 private:
  const MSIHANDLE msi_handle_;
};

MsiHandleImpl::~MsiHandleImpl() = default;
MsiHandleImpl::MsiHandleImpl(MSIHANDLE msi_handle) : msi_handle_(msi_handle) {}

UINT MsiHandleImpl::GetProperty(const std::wstring& name,
                                std::vector<wchar_t>& value,
                                DWORD& value_length) const {
  return ::MsiGetProperty(msi_handle_, name.c_str(), &value[0], &value_length);
}

UINT MsiHandleImpl::SetProperty(const std::string& name,
                                const std::string& value) {
  return ::MsiSetPropertyA(msi_handle_, name.c_str(), value.c_str());
}

MSIHANDLE MsiHandleImpl::CreateRecord(UINT field_count) {
  return ::MsiCreateRecord(field_count);
}

UINT MsiHandleImpl::RecordSetString(MSIHANDLE record_handle,
                                    UINT field_index,
                                    const std::wstring& value) {
  return ::MsiRecordSetString(record_handle, field_index, value.c_str());
}

int MsiHandleImpl::ProcessMessage(INSTALLMESSAGE message_type,
                                  MSIHANDLE record_handle) {
  return ::MsiProcessMessage(msi_handle_, message_type, record_handle);
}

// Gets the value of the property `name` from `msi_handle`.
std::optional<std::wstring> MsiGetProperty(MsiHandleInterface& msi_handle,
                                           const std::wstring& name) {
  DWORD value_length = 0;
  UINT result = ERROR_SUCCESS;
  std::vector<wchar_t> value;
  do {
    value.resize(++value_length);
    result = msi_handle.GetProperty(name, value, value_length);
  } while (result == ERROR_MORE_DATA && value_length <= 0xFFFF);
  return result == ERROR_SUCCESS && !value.empty()
             ? std::make_optional(std::wstring(value.begin(), value.end()))
             : std::nullopt;
}

// If the app installer failed with a custom error and provided a UI string,
// returns that string.
std::optional<std::wstring> GetLastInstallerResultUIString(
    const std::wstring& app_id) {
  if (app_id.empty()) {
    return {};
  }
  auto key = [&app_id]() -> std::optional<base::win::RegKey> {
    if (base::win::RegKey client_state_key(HKEY_LOCAL_MACHINE,
                                           GetAppClientStateKey(app_id).c_str(),
                                           Wow6432(KEY_READ));
        client_state_key.Valid()) {
      return client_state_key;
    }
    if (base::win::RegKey updater_key(HKEY_LOCAL_MACHINE, UPDATER_KEY,
                                      Wow6432(KEY_READ));
        updater_key.Valid()) {
      return updater_key;
    }
    return {};
  }();

  DWORD last_installer_result = 0;
  std::wstring val;
  return key &&
                 key->ReadValueDW(kRegValueLastInstallerResult,
                                  &last_installer_result) == ERROR_SUCCESS &&
                 last_installer_result ==
                     static_cast<DWORD>(InstallerApiResult::kCustomError) &&
                 key->ReadValue(kRegValueLastInstallerResultUIString, &val) ==
                     ERROR_SUCCESS &&
                 !val.empty()
             ? std::make_optional(val)
             : std::nullopt;
}

}  // namespace

UINT MsiSetTags(MsiHandleInterface& msi_handle) {
  const auto msi_path = MsiGetProperty(msi_handle, L"OriginalDatabase");
  if (!msi_path) {
    return ERROR_SUCCESS;
  }

  const auto tag_args =
      updater::tagging::BinaryReadTag(base::FilePath(*msi_path));
  if (!tag_args) {
    return ERROR_SUCCESS;
  }

  msi_handle.SetProperty("TAGSTRING", tag_args->tag_string);
  return ERROR_SUCCESS;
}

UINT MsiSetInstallerResult(MsiHandleInterface& msi_handle) {
  if (const auto app_id = MsiGetProperty(msi_handle, L"CustomActionData");
      app_id) {
    if (const auto result_string = GetLastInstallerResultUIString(*app_id);
        result_string) {
      if (PMSIHANDLE record = msi_handle.CreateRecord(0);
          record && msi_handle.RecordSetString(record, 0, *result_string) ==
                        ERROR_SUCCESS) {
        msi_handle.ProcessMessage(INSTALLMESSAGE_ERROR, record);
      }
    }
  }

  return ERROR_SUCCESS;
}

}  // namespace updater

extern "C" UINT __stdcall ExtractTagInfoFromInstaller(MSIHANDLE msi_handle) {
  updater::MsiHandleImpl msi_handle_impl(msi_handle);
  return updater::MsiSetTags(msi_handle_impl);
}

extern "C" UINT __stdcall ShowInstallerResultUIString(MSIHANDLE msi_handle) {
  updater::MsiHandleImpl msi_handle_impl(msi_handle);
  return updater::MsiSetInstallerResult(msi_handle_impl);
}
