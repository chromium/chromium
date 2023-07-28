// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/installer/msi_custom_action.h"

#include <windows.h>

#include <msi.h>
#include <msiquery.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "chrome/updater/tag.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

// Gets the value of the property `name` from `msi_handle`.
absl::optional<std::wstring> MsiGetProperty(MsiHandleInterface& msi_handle,
                                            const std::wstring& name) {
  DWORD value_length = 0;
  UINT result = ERROR_SUCCESS;
  std::vector<wchar_t> value;
  do {
    value.resize(++value_length);
    result = msi_handle.GetProperty(name, value, value_length);
  } while (result == ERROR_MORE_DATA && value_length <= 0xFFFF);
  return result == ERROR_SUCCESS
             ? absl::make_optional(std::wstring(value.begin(), value.end()))
             : absl::nullopt;
}

}  // namespace

UINT MsiSetTags(MsiHandleInterface& msi_handle) {
  const auto msi_path = MsiGetProperty(msi_handle, L"OriginalDatabase");
  if (!msi_path) {
    return ERROR_SUCCESS;
  }

  const auto tag_args = updater::tagging::MsiReadTag(base::FilePath(*msi_path));
  if (!tag_args) {
    return ERROR_SUCCESS;
  }

  for (const auto& [name, value] : tag_args->attributes) {
    msi_handle.SetProperty(base::ToUpperASCII(name), value);
  }

  return ERROR_SUCCESS;
}

}  // namespace updater

extern "C" UINT __stdcall ExtractTagInfoFromInstaller(MSIHANDLE msi_handle) {
  updater::MsiHandleImpl msi_handle_impl(msi_handle);
  return updater::MsiSetTags(msi_handle_impl);
}
