// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/setup/setup_util.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/win_util.h"
#include "chrome/updater/app/server/win/updater_idl.h"
#include "chrome/updater/util.h"
#include "chrome/updater/win/constants.h"
#include "chrome/updater/win/task_scheduler.h"

namespace updater {

namespace {

constexpr base::char16 kTaskName[] = L"UpdateApps";
constexpr base::char16 kTaskDescription[] = L"Update all applications.";

}  // namespace

bool RegisterWakeTask(const base::CommandLine& run_command) {
  auto task_scheduler = TaskScheduler::CreateInstance();
  if (!task_scheduler->RegisterTask(
          kTaskName, kTaskDescription, run_command,
          TaskScheduler::TriggerType::TRIGGER_TYPE_HOURLY, true)) {
    LOG(ERROR) << "RegisterWakeTask failed.";
    return false;
  }
  VLOG(1) << "RegisterWakeTask succeeded.";
  return true;
}

void UnregisterWakeTask() {
  auto task_scheduler = TaskScheduler::CreateInstance();
  task_scheduler->DeleteTask(kTaskName);
}

base::string16 GetComServerClsidRegistryPath(REFCLSID clsid) {
  return base::StrCat(
      {L"Software\\Classes\\CLSID\\", base::win::WStringFromGUID(clsid)});
}

base::string16 GetComServiceClsid() {
  return base::win::WStringFromGUID(CLSID_UpdaterServiceClass);
}

base::string16 GetComServiceClsidRegistryPath() {
  return base::StrCat({L"Software\\Classes\\CLSID\\", GetComServiceClsid()});
}

base::string16 GetComServiceAppidRegistryPath() {
  return base::StrCat({L"Software\\Classes\\AppID\\", GetComServiceClsid()});
}

base::string16 GetComIidRegistryPath(REFIID iid) {
  return base::StrCat(
      {L"Software\\Classes\\Interface\\", base::win::WStringFromGUID(iid)});
}

base::string16 GetComTypeLibRegistryPath(REFIID iid) {
  return base::StrCat(
      {L"Software\\Classes\\TypeLib\\", base::win::WStringFromGUID(iid)});
}

std::vector<GUID> GetInterfaces() {
  return {
      __uuidof(IAppBundleWeb),
      __uuidof(IAppWeb),
      __uuidof(ICompleteStatus),
      __uuidof(ICurrentState),
      __uuidof(IGoogleUpdate3Web),
      __uuidof(IUpdateState),
      __uuidof(IUpdater),
      __uuidof(IUpdaterControl),
      __uuidof(IUpdaterControlCallback),
      __uuidof(IUpdaterObserver),
  };
}

std::vector<base::FilePath> ParseFilesFromDeps(const base::FilePath& deps) {
  constexpr size_t kDepsFileSizeMax = 0x4000;  // 16KB.
  std::string contents;
  if (!base::ReadFileToStringWithMaxSize(deps, &contents, kDepsFileSizeMax))
    return {};
  const base::flat_set<const base::char16*,
                       CaseInsensitiveASCIICompare<base::string16>>
      exclude_extensions = {L".pdb", L".js"};
  std::vector<base::FilePath> result;
  for (const auto& line :
       base::SplitString(contents, "\r\n", base::TRIM_WHITESPACE,
                         base::SPLIT_WANT_NONEMPTY)) {
    const auto filename =
        base::FilePath(base::ASCIIToUTF16(line)).NormalizePathSeparators();
    if (!base::Contains(exclude_extensions,
                        filename.FinalExtension().c_str())) {
      result.push_back(filename);
    }
  }
  return result;
}

}  // namespace updater
