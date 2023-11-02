// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/extension/user_context_enumerator.h"

#include <windows.h>

#include <map>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/credential_provider/extension/task.h"
#include "chrome/credential_provider/extension/user_device_context.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"

namespace credential_provider {
namespace extension {

// static
UserContextEnumerator** UserContextEnumerator::GetInstanceStorage() {
  static UserContextEnumerator* instance = new UserContextEnumerator();

  return &instance;
}

// static
UserContextEnumerator* UserContextEnumerator::Get() {
  return *GetInstanceStorage();
}

UserContextEnumerator::UserContextEnumerator() {}
UserContextEnumerator::~UserContextEnumerator() {}

HRESULT UserContextEnumerator::PerformTask(const std::string& task_name,
                                           Task& task) {
  std::wstring serial_number = GetSerialNumber();

  std::wstring machine_guid = L"";
  HRESULT hr = GetMachineGuid(&machine_guid);
  if (FAILED(hr))
    LOGFN(WARNING) << "GetMachineGuid failed hr=" << putHR(hr);

  std::map<std::wstring, UserTokenHandleInfo> sid_to_gaia_id;
  hr = GetUserTokenHandles(&sid_to_gaia_id);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "GetUserTokenHandles failed hr=" << putHR(hr);
    return hr;
  }

  if (sid_to_gaia_id.empty()) {
    LOGFN(VERBOSE) << "No GCPW user exists on the device!";
    return S_OK;
  }

  std::vector<UserDeviceContext> context_info;
  for (auto const& entry : sid_to_gaia_id) {
    std::wstring dm_token = L"";
    hr = credential_provider::GetGCPWDmToken(entry.first, &dm_token);
    if (FAILED(hr))
      LOGFN(WARNING) << "GetGCPWDmToken failed hr=" << putHR(hr);

    context_info.push_back({GetUserDeviceResourceId(entry.first), serial_number,
                            machine_guid, entry.first, dm_token});
  }

  hr = task.SetContext(context_info);
  if (FAILED(hr)) {
    LOGFN(ERROR) << task_name << "SetContext hr=" << putHR(hr);
    return hr;
  }

  hr = task.Execute();
  if (FAILED(hr)) {
    LOGFN(ERROR) << task_name << "Execute task hr=" << putHR(hr);
    return hr;
  }

  return S_OK;
}

}  // namespace extension
}  // namespace credential_provider
