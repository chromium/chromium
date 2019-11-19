// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/system_util.h"

#include <windows.h>

#include <objbase.h>
#include <sddl.h>

#include <vector>

#include "base/strings/string_util.h"
#include "base/win/windows_version.h"

namespace chrome_cleaner {

namespace {

// The initial number of services when enumerating services.
const unsigned int kInitialNumberOfServices = 100;

}  // namespace

void SetBackgroundMode() {
  // Get the process working set size and flags, so that we can reset them
  // after setting the process to background mode. For some reason Windows sets
  // a maximum working set size of ~32MB for background processes and prevents
  // them from acquiring more if needed and available (by setting
  // QUOTA_LIMITS_HARDWS_MAX_ENABLE). Returning these values to the default
  // should provide a better user experience since we sometimes need more than
  // 32MB of memory.
  bool got_process_working_set = false;
  SIZE_T minimum_working_set_memory;
  SIZE_T maximum_working_set_memory;
  DWORD working_set_flags;
  if (GetProcessWorkingSetSizeEx(
          GetCurrentProcess(), &minimum_working_set_memory,
          &maximum_working_set_memory, &working_set_flags)) {
    got_process_working_set = true;
  } else {
    PLOG(ERROR) << "Failed to call GetProcessWorkingSetSizeEx";
  }

  if (!SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS)) {
    PLOG(ERROR) << "Can't SetPriorityClass to BELOW_NORMAL_PRIORITY_CLASS";
  }

  if (!SetPriorityClass(GetCurrentProcess(), PROCESS_MODE_BACKGROUND_BEGIN)) {
    PLOG(ERROR) << "Can't SetPriorityClass to PROCESS_MODE_BACKGROUND_BEGIN";
  }

  if (got_process_working_set) {
    if (!SetProcessWorkingSetSizeEx(
            GetCurrentProcess(), minimum_working_set_memory,
            maximum_working_set_memory, working_set_flags)) {
      PLOG(ERROR) << "Failed to call SetProcessWorkingSetSizeEx";
    }
  }
}
bool IsWowRedirectionActive() {
  return base::win::OSInfo::GetInstance()->wow64_status() ==
         base::win::OSInfo::WOW64_ENABLED;
}

bool IsX64Architecture() {
  return base::win::OSInfo::GetArchitecture() ==
         base::win::OSInfo::X64_ARCHITECTURE;
}

bool IsX64Process() {
  // WOW64 redirection is enabled for 32-bit processes on 64-bit Windows.
  return IsX64Architecture() && !IsWowRedirectionActive();
}

bool EnumerateServices(const ScopedScHandle& service_manager,
                       DWORD service_type,
                       DWORD service_state,
                       std::vector<ServiceStatus>* services) {
  DCHECK(services);
  std::vector<uint8_t> buffer(kInitialNumberOfServices *
                              sizeof(ENUM_SERVICE_STATUS_PROCESS));
  ULONG number_of_services = 0;
  ULONG more_bytes_needed = 0;
  while (!::EnumServicesStatusEx(service_manager.Get(), SC_ENUM_PROCESS_INFO,
                                 service_type, service_state, buffer.data(),
                                 buffer.size(), &more_bytes_needed,
                                 &number_of_services, nullptr, nullptr)) {
    if (::GetLastError() != ERROR_MORE_DATA) {
      PLOG(ERROR) << "Cannot enumerate running services.";
      return false;
    }
    buffer.resize(buffer.size() + more_bytes_needed);
  }

  services->reserve(number_of_services);
  ENUM_SERVICE_STATUS_PROCESS* service_array =
      reinterpret_cast<ENUM_SERVICE_STATUS_PROCESS*>(buffer.data());
  for (size_t i = 0; i < number_of_services; i++) {
    services->push_back(ServiceStatus{service_array[i].lpServiceName,
                                      service_array[i].lpDisplayName,
                                      service_array[i].ServiceStatusProcess});
  }
  return true;
}

}  // namespace chrome_cleaner
