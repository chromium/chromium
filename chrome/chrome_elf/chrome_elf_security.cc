// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_elf/chrome_elf_security.h"

// clang-format off
#include <windows.h> // Must be included before versionhelpers.h
#include <versionhelpers.h>
// clang-format on

#include <assert.h>
#include <ntstatus.h>

#include <optional>

#include "base/check.h"
#include "base/file_version_info.h"
#include "base/logging.h"
#include "base/threading/thread_checker.h"
#include "base/win/current_module.h"
#include "chrome/chrome_elf/chrome_elf_constants.h"
#include "chrome/chrome_elf/nt_registry/nt_registry.h"
#include "chrome/install_static/install_util.h"

namespace elf_security {

namespace {

// Used to turn off validation in tests
static bool g_validate_not_exe_for_testing = true;

// Used to ensure we are calling a method defined in the correct module.
// In particular, there are often issues where the exe version is called
// instead of the dll version.
void MaybeValidateNotCallingFromExe() {
  HMODULE module;

  // NULL returns the exe we're running in
  DCHECK(::GetModuleHandleExW(0, NULL, &module));
  DCHECK(CURRENT_MODULE() != module);
}

class ExtensionPointDisableSet {
 public:
  ~ExtensionPointDisableSet() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  }

  ExtensionPointDisableSet(const ExtensionPointDisableSet&) = delete;
  ExtensionPointDisableSet& operator=(const ExtensionPointDisableSet&) = delete;

  static ExtensionPointDisableSet* GetInstance() {
    static ExtensionPointDisableSet* instance = nullptr;
    if (!instance) {
      instance = new ExtensionPointDisableSet();
    }

    return instance;
  }

  void SetExtensionPointDisabled(bool set) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    CHECK(!extension_point_disable_set_.has_value());
    extension_point_disable_set_ = set;
  }

  bool GetExtensionPointDisabled() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    if (extension_point_disable_set_.has_value())
      return extension_point_disable_set_.value();

    return false;
  }

 private:
  ExtensionPointDisableSet() { DETACH_FROM_THREAD(thread_checker_); }

  THREAD_CHECKER(thread_checker_);
  std::optional<bool> extension_point_disable_set_
      GUARDED_BY_CONTEXT(thread_checker_);
};

}  // namespace

void EarlyBrowserSecurity() {
  // This function is called from within DllMain.
  // Don't do anything naughty while we have the loader lock.
  NTSTATUS ret_val = STATUS_SUCCESS;
  HANDLE handle = INVALID_HANDLE_VALUE;

  // Check for kRegBrowserExtensionPointKeyName. We only disable extension
  // points when this exists, for devices that have been vetted by our
  // heuristic.
  if (!nt::OpenRegKey(nt::HKCU,
                      install_static::GetRegistryPath()
                          .append(elf_sec::kRegBrowserExtensionPointKeyName)
                          .c_str(),
                      KEY_QUERY_VALUE, &handle, &ret_val)) {
#ifdef _DEBUG
    // The only failure expected is for the path not existing.
    if (ret_val != STATUS_OBJECT_NAME_NOT_FOUND)
      assert(false);
#endif
    return;
  }

  nt::CloseRegKey(handle);

  // Disable extension points (legacy hooking) in this process.
  PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY policy = {};
  policy.DisableExtensionPoints = true;
  SetProcessMitigationPolicy(ProcessExtensionPointDisablePolicy, &policy,
                             sizeof(policy));
  ExtensionPointDisableSet::GetInstance()->SetExtensionPointDisabled(true);

  return;
}

void ValidateExeForTesting(bool on) {
  g_validate_not_exe_for_testing = on;
}

bool IsExtensionPointDisableSet() {
  if (g_validate_not_exe_for_testing)
    MaybeValidateNotCallingFromExe();

  return ExtensionPointDisableSet::GetInstance()->GetExtensionPointDisabled();
}

}  // namespace elf_security
