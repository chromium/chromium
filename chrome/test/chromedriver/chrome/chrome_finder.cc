// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/chrome_finder.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/chrome_constants.h"

#if BUILDFLAG(IS_WIN)
#include "base/base_paths_win.h"
#include "base/win/windows_version.h"
#endif

namespace {

#if BUILDFLAG(IS_WIN)
void GetApplicationDirs(std::vector<base::FilePath>* locations) {
  std::vector<base::FilePath> installation_locations;
  base::FilePath local_app_data, program_files, program_files_x86,
      program_files_64_32;
  if (base::PathService::Get(base::DIR_LOCAL_APP_DATA, &local_app_data))
    installation_locations.push_back(local_app_data);
  if (base::PathService::Get(base::DIR_PROGRAM_FILES, &program_files))
    installation_locations.push_back(program_files);
  if (base::PathService::Get(base::DIR_PROGRAM_FILESX86, &program_files_x86))
    installation_locations.push_back(program_files_x86);
  if (base::PathService::Get(base::DIR_PROGRAM_FILES6432, &program_files_64_32))
    installation_locations.push_back(program_files_64_32);

  for (size_t i = 0; i < installation_locations.size(); ++i) {
    locations->push_back(
        installation_locations[i].Append(L"Google\\Chrome\\Application"));
  }
  for (size_t i = 0; i < installation_locations.size(); ++i) {
    locations->push_back(installation_locations[i].Append(
        L"Google\\Chrome for Testing\\Application"));
  }
  for (size_t i = 0; i < installation_locations.size(); ++i) {
    locations->push_back(
        installation_locations[i].Append(L"Chromium\\Application"));
  }
}
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
void GetApplicationDirs(std::vector<base::FilePath>* locations) {
  // TODO: Respect users' PATH variables.
  // Until then, we use an approximation of the most common defaults.
  locations->push_back(base::FilePath("/usr/local/sbin"));
  locations->push_back(base::FilePath("/usr/local/bin"));
  locations->push_back(base::FilePath("/usr/sbin"));
  locations->push_back(base::FilePath("/usr/bin"));
  locations->push_back(base::FilePath("/sbin"));
  locations->push_back(base::FilePath("/bin"));
  // Lastly, try the default installation location.
  locations->push_back(base::FilePath("/opt/google/chrome"));
  locations->push_back(base::FilePath("/opt/chromium.org/chromium"));
}
#elif BUILDFLAG(IS_ANDROID)
void GetApplicationDirs(std::vector<base::FilePath>* locations) {
  // On Android we won't be able to find Chrome executable
}
#endif

void GetPathsFromEnvironment(std::vector<base::FilePath>* paths) {
  base::FilePath::StringType delimiter;
  base::FilePath::StringType common_path;
  std::string path;
  std::unique_ptr<base::Environment> env(base::Environment::Create());

  if (!env->GetVar("PATH", &path)) {
    return;
  }

#if BUILDFLAG(IS_WIN)
  common_path = base::UTF8ToWide(path);
  delimiter = L";";
#else
  common_path = path;
  delimiter = ":";
#endif

  std::vector<base::FilePath::StringType> path_entries = base::SplitString(
      common_path, delimiter, base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  for (auto& path_entry : path_entries) {
#if BUILDFLAG(IS_WIN)
    size_t size = path_entry.size();
    if (size >= 2 && path_entry[0] == '"' && path_entry[size - 1] == '"') {
      path_entry.erase(0, 1);
      path_entry.erase(size - 2, 1);
    }
#endif
    if (path_entry.size() > 0)
      paths->emplace_back(path_entry);
  }
}

}  // namespace

namespace internal {

bool FindExe(
    const base::RepeatingCallback<bool(const base::FilePath&)>& exists_func,
    const std::vector<base::FilePath>& rel_paths,
    const std::vector<base::FilePath>& locations,
    base::FilePath* out_path) {
  for (auto& rel_path : rel_paths) {
    for (auto& location : locations) {
      base::FilePath path = location.Append(rel_path);
      if (exists_func.Run(path)) {
        *out_path = path;
        return true;
      }
    }
  }
  return false;
}

}  // namespace internal

#if BUILDFLAG(IS_MAC)
void GetApplicationDirs(std::vector<base::FilePath>* locations);
#endif

bool FindChrome(base::FilePath* browser_exe) {
  base::FilePath browser_exes_array[] = {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
    base::FilePath(chrome::kBrowserProcessExecutablePath),
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    base::FilePath("google-chrome"),
    base::FilePath(chrome::kBrowserProcessExecutablePath),
    base::FilePath("chromium"),
    base::FilePath("chromium-browser")
#else
    // it will compile but won't work on other OSes
    base::FilePath()
#endif
  };

  LOG_IF(ERROR, browser_exes_array[0].empty()) << "Unsupported platform.";

  std::vector<base::FilePath> browser_exes(
      browser_exes_array, browser_exes_array + std::size(browser_exes_array));
  base::FilePath module_dir;
#if BUILDFLAG(IS_FUCHSIA)
  // Use -1 to allow this to compile.
  // TODO(crbug.com/1262176): Determine whether Fuchsia should support this and
  // if so provide an appropriate implementation for this function.
  if (base::PathService::Get(-1, &module_dir)) {
#else
  if (base::PathService::Get(base::DIR_MODULE, &module_dir)) {
#endif
    for (const base::FilePath& file_path : browser_exes) {
      base::FilePath path = module_dir.Append(file_path);
      if (base::PathExists(path)) {
        *browser_exe = path;
        return true;
      }
    }
  }

  std::vector<base::FilePath> locations;
  GetApplicationDirs(&locations);
  GetPathsFromEnvironment(&locations);
  return internal::FindExe(base::BindRepeating(&base::PathExists), browser_exes,
                           locations, browser_exe);
}
