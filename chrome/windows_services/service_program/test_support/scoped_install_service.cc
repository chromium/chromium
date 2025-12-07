// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/service_program/test_support/scoped_install_service.h"

#include <windows.h>

#include <string>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/check.h"
#include "base/containers/heap_array.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/install_service_work_item.h"

ScopedInstallService::ScopedInstallService(std::wstring_view service_name,
                                           std::wstring_view display_name,
                                           std::wstring_view description,
                                           base::CommandLine service_command,
                                           const CLSID& clsid,
                                           const IID& iid) {
  const std::wstring name(service_name);
  const std::wstring display(display_name);
  const std::wstring description_text(description);
  const std::vector<GUID> clsids{clsid};
  const std::vector<GUID> iids{iid};

  // Delete an old instance if one was left behind by a previous crash.
  installer::InstallServiceWorkItem::DeleteService(name, display, clsids, iids);

  static constexpr const char* kSwitchesToCopy[] = {
      switches::kV,
      switches::kVModule,
  };
  service_command.CopySwitchesFrom(*base::CommandLine::ForCurrentProcess(),
                                   kSwitchesToCopy);
  auto work_item = std::make_unique<installer::InstallServiceWorkItem>(
      name, display, description_text, SERVICE_DEMAND_START,
      std::move(service_command),
      base::CommandLine(base::CommandLine::NO_PROGRAM),
      install_static::GetClientStateKeyPath(), clsids, iids);
  if (work_item->Do()) {
    service_name_ = std::move(name);
    work_item_ = std::move(work_item);
  }
}

ScopedInstallService::~ScopedInstallService() {
  if (work_item_) {
    work_item_->Rollback();
  }
}

namespace {

struct ScHandleCloser {
  void operator()(SC_HANDLE handle) const { ::CloseServiceHandle(handle); }
};

using ScopedScHandle = std::unique_ptr<SC_HANDLE__, ScHandleCloser>;

}  // namespace

base::Process ScopedInstallService::GetRunningService() {
  if (service_name_.empty()) {
    return base::Process();
  }

  ScopedScHandle scm(::OpenSCManager(/*lpMachineName=*/nullptr,
                                     /*lpDatabaseName=*/nullptr,
                                     SC_MANAGER_CONNECT));
  CHECK(scm);

  ScopedScHandle service(
      ::OpenService(scm.get(), service_name_.c_str(), SERVICE_QUERY_STATUS));
  CHECK(service);

  // MSDN says that the max allowed buffer size is 8k.
  auto buffer = base::HeapArray<uint8_t>::Uninit(1024 * 8);
  auto* const status = reinterpret_cast<SERVICE_STATUS_PROCESS*>(buffer.data());
  DWORD bytes_needed = 0;
  CHECK(::QueryServiceStatusEx(service.get(), SC_STATUS_PROCESS_INFO,
                               buffer.data(), buffer.size(), &bytes_needed));
  CHECK_LE(bytes_needed, buffer.size());

  if (status->dwCurrentState != SERVICE_STOPPED && status->dwProcessId) {
    return base::Process::OpenWithAccess(status->dwProcessId, SYNCHRONIZE);
  }
  return base::Process();
}
