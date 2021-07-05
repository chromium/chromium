// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_elf/chrome_elf_security.h"

#include <windows.h>

#include <assert.h>
#include <versionhelpers.h>  // windows.h must be before

#include "chrome/chrome_elf/chrome_elf_constants.h"
#include "chrome/chrome_elf/nt_registry/nt_registry.h"
#include "chrome/install_static/install_util.h"

namespace elf_security {

void EarlyBrowserSecurity() {
  typedef decltype(SetProcessMitigationPolicy)* SetProcessMitigationPolicyFunc;

  // This function is called from within DllMain.
  // Don't do anything naughty while we have the loader lock.
  NTSTATUS ret_val = STATUS_SUCCESS;
  HANDLE handle = INVALID_HANDLE_VALUE;

  // Check for kRegistrySecurityFinchPath.  If it exists,
  // we do NOT disable extension points.  (Emergency off flag.)
  if (nt::OpenRegKey(nt::HKCU,
                     install_static::GetRegistryPath()
                         .append(elf_sec::kRegSecurityFinchKeyName)
                         .c_str(),
                     KEY_QUERY_VALUE, &handle, &ret_val)) {
    nt::CloseRegKey(handle);
    return;
  }
#ifdef _DEBUG
  // The only failure expected is for the path not existing.
  if (ret_val != STATUS_OBJECT_NAME_NOT_FOUND)
    assert(false);
#endif

  if (::IsWindows8OrGreater()) {
    SetProcessMitigationPolicyFunc set_process_mitigation_policy =
        reinterpret_cast<SetProcessMitigationPolicyFunc>(::GetProcAddress(
            ::GetModuleHandleW(L"kernel32.dll"), "SetProcessMitigationPolicy"));
    if (set_process_mitigation_policy) {
      // Disable extension points in this process.
      // (Legacy hooking.)
      PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY policy = {};
      policy.DisableExtensionPoints = true;
      set_process_mitigation_policy(ProcessExtensionPointDisablePolicy, &policy,
                                    sizeof(policy));
    }
  }
  return;
}
}  // namespace elf_security
