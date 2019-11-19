// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_ELF_HOOK_UTIL_HOOK_UTIL_H_
#define CHROME_CHROME_ELF_HOOK_UTIL_HOOK_UTIL_H_

#include <windows.h>

#include "base/macros.h"

namespace sandbox {
class ServiceResolverThunk;
}

namespace elf_hook {

//------------------------------------------------------------------------------
// System Service hooking support
//------------------------------------------------------------------------------

// Creates a |ServiceResolverThunk| based on the OS version. Ownership of the
// resulting thunk is passed to the caller.
sandbox::ServiceResolverThunk* HookSystemService(bool relaxed);

//------------------------------------------------------------------------------
// Import Address Table hooking support
//------------------------------------------------------------------------------
class IATHook {
 public:
  IATHook();
  ~IATHook();

  // Intercept a function in an import table of a specific
  // module. Saves everything needed to Unhook.
  //
  // NOTE: Hook can only be called once at a time, to enable Unhook().
  //
  // Arguments:
  // module                 Module to be intercepted
  // imported_from_module   Module that exports the 'function_name'
  // function_name          Name of the API to be intercepted
  // new_function           New function pointer
  //
  // Returns: Windows error code (winerror.h). NO_ERROR if successful.
  DWORD Hook(HMODULE module,
             const char* imported_from_module,
             const char* function_name,
             void* new_function);

  // Unhook the IAT entry.
  //
  // Returns: Windows error code (winerror.h). NO_ERROR if successful.
  DWORD Unhook();

 private:
  void* intercept_function_;
  void* original_function_;
  IMAGE_THUNK_DATA* iat_thunk_;

  DISALLOW_COPY_AND_ASSIGN(IATHook);
};

}  // namespace elf_hook

#endif  // CHROME_ELF_HOOK_UTIL_HOOK_UTIL_H_
