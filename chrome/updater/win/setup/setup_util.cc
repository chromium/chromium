// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/setup/setup_util.h"

#include <string>
#include <unordered_map>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/win_util.h"
#include "chrome/updater/app/server/win/updater_idl.h"
#include "chrome/updater/app/server/win/updater_internal_idl.h"
#include "chrome/updater/app/server/win/updater_legacy_idl.h"
#include "chrome/updater/util.h"
#include "chrome/updater/win/constants.h"
#include "chrome/updater/win/task_scheduler.h"

// Specialization for std::hash so that IID instances can be stored in an
// associative container. This implementation of the hash function adds
// together four 32-bit integers which make up an IID. The function does not
// have to be efficient or guarantee no collisions. It is used infrequently,
// for a small number of IIDs, and the container deals with collisions.
template <>
struct std::hash<IID> {
  size_t operator()(const IID& iid) const {
    return iid.Data1 + (iid.Data2 + (iid.Data3 << 16)) + [&iid]() {
      size_t val = 0;
      for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 4; ++j) {
          val += (iid.Data4[j + i * 4] << (j * 4));
        }
      }
      return val;
    }();
  }
};

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
  return base::win::WStringFromGUID(__uuidof(UpdaterServiceClass));
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

base::string16 GetComTypeLibResourceIndex(REFIID iid) {
  // These values must be kept in sync with the numeric typelib resource
  // indexes in the resource file.
  constexpr base::char16 kUpdaterIndex[] = L"1";
  constexpr base::char16 kUpdaterInternalIndex[] = L"2";
  constexpr base::char16 kUpdaterLegacyIndex[] = L"3";

  static const std::unordered_map<IID, const base::char16*> kTypeLibIndexes = {
      // Updater typelib.
      {__uuidof(ICompleteStatus), kUpdaterIndex},
      {__uuidof(IUpdater), kUpdaterIndex},
      {__uuidof(IUpdaterObserver), kUpdaterIndex},
      {__uuidof(IUpdateState), kUpdaterIndex},

      // Updater internal typelib.
      {__uuidof(IUpdaterInternal), kUpdaterInternalIndex},
      {__uuidof(IUpdaterInternalCallback), kUpdaterInternalIndex},

      // Updater legacy typelib.
      {__uuidof(IAppBundleWeb), kUpdaterLegacyIndex},
      {__uuidof(IAppWeb), kUpdaterLegacyIndex},
      {__uuidof(ICurrentState), kUpdaterLegacyIndex},
      {__uuidof(IGoogleUpdate3Web), kUpdaterLegacyIndex},
  };
  auto index = kTypeLibIndexes.find(iid);
  return index != kTypeLibIndexes.end() ? index->second : L"";
}

std::vector<GUID> GetInterfaces() {
  static const std::vector<GUID> kInterfaces = {
      __uuidof(IAppBundleWeb),
      __uuidof(IAppWeb),
      __uuidof(ICompleteStatus),
      __uuidof(ICurrentState),
      __uuidof(IGoogleUpdate3Web),
      __uuidof(IUpdateState),
      __uuidof(IUpdater),
      __uuidof(IUpdaterInternal),
      __uuidof(IUpdaterInternalCallback),
      __uuidof(IUpdaterObserver),
  };
  return kInterfaces;
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
