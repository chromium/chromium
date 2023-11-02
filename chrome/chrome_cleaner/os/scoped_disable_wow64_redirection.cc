// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/scoped_disable_wow64_redirection.h"

#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/scoped_native_library.h"

namespace chrome_cleaner {
namespace {

// A helper class encapsulating run-time-linked function calls to Wow64 APIs.
class Wow64Functions {
 public:
  Wow64Functions()
      : kernel32_lib_(base::FilePath(L"kernel32")),
        is_wow_64_process_(nullptr),
        wow_64_disable_wow_64_fs_redirection_(nullptr),
        wow_64_revert_wow_64_fs_redirection_(nullptr) {
    if (kernel32_lib_.is_valid()) {
      is_wow_64_process_ = reinterpret_cast<IsWow64Process>(
          kernel32_lib_.GetFunctionPointer("IsWow64Process"));
      wow_64_disable_wow_64_fs_redirection_ =
          reinterpret_cast<Wow64DisableWow64FSRedirection>(
              kernel32_lib_.GetFunctionPointer(
                  "Wow64DisableWow64FsRedirection"));
      wow_64_revert_wow_64_fs_redirection_ =
          reinterpret_cast<Wow64RevertWow64FSRedirection>(
              kernel32_lib_.GetFunctionPointer(
                  "Wow64RevertWow64FsRedirection"));
    } else {
      PLOG(ERROR) << "Cannot open library 'kernel32'.";
    }
  }

  Wow64Functions(const Wow64Functions&) = delete;
  Wow64Functions& operator=(const Wow64Functions&) = delete;

  bool is_valid() const {
    return is_wow_64_process_ && wow_64_disable_wow_64_fs_redirection_ &&
           wow_64_revert_wow_64_fs_redirection_;
  }

  bool IsWow64() const {
    BOOL result = 0;
    if (!is_wow_64_process_(GetCurrentProcess(), &result))
      PLOG(WARNING) << "IsWow64Process";
    return !!result;
  }

  bool DisableFsRedirection(PVOID* previous_state) {
    return !!wow_64_disable_wow_64_fs_redirection_(previous_state);
  }

  bool RevertFsRedirection(PVOID previous_state) {
    return !!wow_64_revert_wow_64_fs_redirection_(previous_state);
  }

 private:
  typedef BOOL(WINAPI* IsWow64Process)(HANDLE, PBOOL);
  typedef BOOL(WINAPI* Wow64DisableWow64FSRedirection)(PVOID*);
  typedef BOOL(WINAPI* Wow64RevertWow64FSRedirection)(PVOID);

  base::ScopedNativeLibrary kernel32_lib_;

  IsWow64Process is_wow_64_process_;
  Wow64DisableWow64FSRedirection wow_64_disable_wow_64_fs_redirection_;
  Wow64RevertWow64FSRedirection wow_64_revert_wow_64_fs_redirection_;
};

// Global Wow64Function instance used by ScopedDisableWow64Redirection below.
base::LazyInstance<Wow64Functions>::Leaky g_wow_64_functions =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

ScopedDisableWow64Redirection::ScopedDisableWow64Redirection()
    : active_(false), previous_state_(nullptr) {
  Wow64Functions* wow64 = g_wow_64_functions.Pointer();
  if (wow64->is_valid() && wow64->IsWow64()) {
    if (wow64->DisableFsRedirection(&previous_state_))
      active_ = true;
    else
      PLOG(WARNING) << "Wow64DisableWow64FSRedirection";
  }
}

ScopedDisableWow64Redirection::~ScopedDisableWow64Redirection() {
  if (active_) {
    // The Wow64 redirection needs to be reverted. In case of a failure, the
    // process is in an invalid state where every access to the system folder
    // will occurs to the wrong folder. This is unlikely to happen.
    CHECK(g_wow_64_functions.Get().RevertFsRedirection(previous_state_));
  }
}

}  // namespace chrome_cleaner
