// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/process.h"

#include <windows.h>

#include <psapi.h>

#include <vector>

#include "base/logging.h"

namespace chrome_cleaner {

bool GetLoadedModuleFileNames(HANDLE process,
                              std::set<std::wstring>* module_names) {
  std::vector<HMODULE> module_handles;
  size_t modules_count = 128;
  // Adjust array size for all modules to fit into it.
  do {
    // Allocate more space than needed in case more modules were loaded between
    // calls to EnumProcessModulesEx.
    modules_count *= 2;
    module_handles.resize(modules_count);
    DWORD bytes_needed;
    if (!::EnumProcessModulesEx(process, module_handles.data(),
                                modules_count * sizeof(HMODULE), &bytes_needed,
                                LIST_MODULES_ALL)) {
      PLOG(ERROR) << "Failed to enumerate modules";
      return false;
    }
    DCHECK_EQ(bytes_needed % sizeof(HMODULE), 0U);
    modules_count = bytes_needed / sizeof(HMODULE);
  } while (modules_count > module_handles.size());

  DCHECK(module_names);
  wchar_t module_name[MAX_PATH];
  for (size_t i = 0; i < modules_count; ++i) {
    size_t module_name_length = ::GetModuleFileNameExW(
        process, module_handles[i], module_name, MAX_PATH);
    if (module_name_length == 0) {
      PLOG(ERROR) << "Failed to get module filename";
      continue;
    }
    module_names->insert(std::wstring(module_name, module_name_length));
  }
  return true;
}

bool GetProcessExecutablePath(HANDLE process, std::wstring* path) {
  DCHECK(path);

  std::vector<wchar_t> image_path(MAX_PATH);
  DWORD path_length = image_path.size();
  BOOL success =
      ::QueryFullProcessImageName(process, 0, image_path.data(), &path_length);
  if (!success && ::GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
    // Process name is potentially greater than MAX_PATH, try larger max size.
    image_path.resize(SHRT_MAX);
    path_length = image_path.size();
    success = ::QueryFullProcessImageName(process, 0, image_path.data(),
                                          &path_length);
  }
  if (!success) {
    PLOG_IF(ERROR, ::GetLastError() != ERROR_GEN_FAILURE)
        << "Failed to get process image path";
    return false;
  }
  path->assign(image_path.data(), path_length);
  return true;
}

bool GetSystemResourceUsage(HANDLE process, SystemResourceUsage* stats) {
  DCHECK(stats);

  FILETIME creation_time;
  FILETIME exit_time;
  FILETIME kernel_time;
  FILETIME user_time;
  // The returned user and kernel times are to be interpreted as time
  // durations and not as time points. |base::Time| can convert from FILETIME
  // to |base::Time| and by subtracting from time zero, we get the duration as
  // a |base::TimeDelta|.
  if (!::GetProcessTimes(process, &creation_time, &exit_time, &kernel_time,
                         &user_time)) {
    PLOG(ERROR) << "Could not get process times";
    return false;
  }
  stats->kernel_time = base::Time::FromFileTime(kernel_time) - base::Time();
  stats->user_time = base::Time::FromFileTime(user_time) - base::Time();

  std::unique_ptr<base::ProcessMetrics> metrics(
      base::ProcessMetrics::CreateProcessMetrics(process));
  if (!metrics || !metrics->GetIOCounters(&stats->io_counters)) {
    LOG(ERROR) << "Failed to get IO process counters";
    return false;
  }

  PROCESS_MEMORY_COUNTERS pmc;
  if (::GetProcessMemoryInfo(::GetCurrentProcess(), &pmc, sizeof(pmc))) {
    stats->peak_working_set_size = pmc.PeakWorkingSetSize;
  }

  return true;
}

}  // namespace chrome_cleaner
