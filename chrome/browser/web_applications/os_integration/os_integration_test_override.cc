// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/os_integration_test_override.h"

#include <memory>
#include <tuple>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_registration.h"
#include "chrome/browser/web_applications/web_app_id.h"

namespace web_app {

namespace {

struct OsIntegrationTestOverrideState {
  base::Lock lock;
  raw_ptr<OsIntegrationTestOverride> global_os_integration_test_override
      GUARDED_BY(lock) = nullptr;
};

OsIntegrationTestOverrideState&
GetMutableOsIntegrationTestOverrideStateForTesting() {
  static base::NoDestructor<OsIntegrationTestOverrideState>
      g_os_integration_test_override;
  return *g_os_integration_test_override.get();
}

std::string GetAllFilesInDir(const base::FilePath& file_path) {
  std::vector<std::string> files_as_strs;
  base::FileEnumerator files(file_path, true, base::FileEnumerator::FILES);
  for (base::FilePath current = files.Next(); !current.empty();
       current = files.Next()) {
    files_as_strs.push_back(current.AsUTF8Unsafe());
  }
  return base::JoinString(base::make_span(files_as_strs), "\n  ");
}

}  // namespace

OsIntegrationTestOverride::BlockingRegistration::BlockingRegistration() =
    default;
OsIntegrationTestOverride::BlockingRegistration::~BlockingRegistration() {
  base::ScopedAllowBlockingForTesting blocking;
  base::RunLoop wait_until_destruction_loop;
  // Lock the global state.
  {
    auto& global_state = GetMutableOsIntegrationTestOverrideStateForTesting();
    base::AutoLock state_lock(global_state.lock);
    DCHECK_EQ(global_state.global_os_integration_test_override,
              test_override.get());

    // Set the destruction closure for the scoped override object.
    DCHECK(!test_override->on_destruction_)
        << "Cannot have multiple registrations at the same time.";
    test_override->on_destruction_.ReplaceClosure(
        wait_until_destruction_loop.QuitClosure());

    // Unregister the override so new handles cannot be acquired.
    global_state.global_os_integration_test_override = nullptr;
  }

  // Release the override & wait until all references are released.
  // Note: The `test_override` MUST be released before waiting on the run
  // loop, as then it will hang forever.
  test_override.reset();
  wait_until_destruction_loop.Run();
}

// static
std::unique_ptr<OsIntegrationTestOverride::BlockingRegistration>
OsIntegrationTestOverride::OverrideForTesting(const base::FilePath& base_path) {
  auto& state = GetMutableOsIntegrationTestOverrideStateForTesting();
  base::AutoLock state_lock(state.lock);
  DCHECK(!state.global_os_integration_test_override)
      << "Cannot have multiple registrations at the same time.";
  auto test_override =
      base::WrapRefCounted(new OsIntegrationTestOverride(base_path));
  state.global_os_integration_test_override = test_override.get();

  std::unique_ptr<BlockingRegistration> registration =
      std::make_unique<BlockingRegistration>();
  registration->test_override = test_override;
  return registration;
}

OsIntegrationTestOverride::OsIntegrationTestOverride(
    const base::FilePath& base_path) {
  // Initialize all directories used. The success & the DCHECK are separated to
  // ensure that these function calls occur on release builds.
  if (!base_path.empty()) {
#if BUILDFLAG(IS_WIN)
    bool success = desktop_.CreateUniqueTempDirUnderPath(base_path);
    DCHECK(success);
    success = application_menu_.CreateUniqueTempDirUnderPath(base_path);
    DCHECK(success);
    success = quick_launch_.CreateUniqueTempDirUnderPath(base_path);
    DCHECK(success);
    success = startup_.CreateUniqueTempDirUnderPath(base_path);
    DCHECK(success);
#elif BUILDFLAG(IS_MAC)
    bool success = chrome_apps_folder_.CreateUniqueTempDirUnderPath(base_path);
    DCHECK(success);
#elif BUILDFLAG(IS_LINUX)
    bool success = desktop_.CreateUniqueTempDirUnderPath(base_path);
    DCHECK(success);
    success = startup_.CreateUniqueTempDirUnderPath(base_path);
    DCHECK(success);
#endif
  } else {
#if BUILDFLAG(IS_WIN)
    bool success = desktop_.CreateUniqueTempDir();
    DCHECK(success);
    success = application_menu_.CreateUniqueTempDir();
    DCHECK(success);
    success = quick_launch_.CreateUniqueTempDir();
    DCHECK(success);
    success = startup_.CreateUniqueTempDir();
    DCHECK(success);
#elif BUILDFLAG(IS_MAC)
    bool success = chrome_apps_folder_.CreateUniqueTempDir();
    DCHECK(success);
#elif BUILDFLAG(IS_LINUX)
    bool success = desktop_.CreateUniqueTempDir();
    DCHECK(success);
    success = startup_.CreateUniqueTempDir();
    DCHECK(success);
#endif
  }

#if BUILDFLAG(IS_LINUX)
  auto callback =
      base::BindRepeating([](base::FilePath filename, std::string xdg_command,
                             std::string file_contents) {
        auto test_override = GetOsIntegrationTestOverride();
        DCHECK(test_override);
        LinuxFileRegistration file_registration = LinuxFileRegistration();
        file_registration.xdg_command = xdg_command;
        file_registration.file_contents = file_contents;
        test_override->linux_file_registration_.push_back(file_registration);
        return true;
      });
  SetUpdateMimeInfoDatabaseOnLinuxCallbackForTesting(std::move(callback));
#endif
}

OsIntegrationTestOverride::~OsIntegrationTestOverride() {
  std::vector<base::ScopedTempDir*> directories;
#if BUILDFLAG(IS_WIN)
  directories = {&desktop_, &application_menu_, &quick_launch_, &startup_};
#elif BUILDFLAG(IS_MAC)
  directories = {&chrome_apps_folder_};
  // Checks and cleans up possible hidden files in directories.
  std::vector<std::string> hidden_files{"Icon\r", ".localized"};
  for (base::ScopedTempDir* dir : directories) {
    if (dir->IsValid()) {
      for (auto& f : hidden_files) {
        base::FilePath path = dir->GetPath().Append(f);
        if (base::PathExists(path)) {
          base::DeletePathRecursively(path);
        }
      }
    }
  }
#elif BUILDFLAG(IS_LINUX)
  // Reset the file handling callback.
  SetUpdateMimeInfoDatabaseOnLinuxCallbackForTesting(
      UpdateMimeInfoDatabaseOnLinuxCallback());
  directories = {&desktop_};
#endif
  for (base::ScopedTempDir* dir : directories) {
    if (!dir->IsValid()) {
      continue;
    }
    DCHECK(base::IsDirectoryEmpty(dir->GetPath()))
        << "Directory not empty: " << dir->GetPath().AsUTF8Unsafe()
        << ". Please uninstall all webapps that have been installed while "
           "shortcuts were overriden. Contents:\n"
        << GetAllFilesInDir(dir->GetPath());
  }
}

scoped_refptr<OsIntegrationTestOverride> GetOsIntegrationTestOverride() {
  auto& state = GetMutableOsIntegrationTestOverrideStateForTesting();
  base::AutoLock state_lock(state.lock);
  return base::WrapRefCounted(state.global_os_integration_test_override.get());
}

}  // namespace web_app
