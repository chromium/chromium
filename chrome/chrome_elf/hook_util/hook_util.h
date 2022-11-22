// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_ELF_HOOK_UTIL_HOOK_UTIL_H_
#define CHROME_CHROME_ELF_HOOK_UTIL_HOOK_UTIL_H_

#include <windows.h>

namespace elf_hook {

//------------------------------------------------------------------------------
// Import Address Table hooking support
//------------------------------------------------------------------------------
class IATHook {
 public:
  IATHook();

  IATHook(const IATHook&) = delete;
  IATHook& operator=(const IATHook&) = delete;

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
};

}  // namespace elf_hook

#endif  // CHROME_CHROME_ELF_HOOK_UTIL_HOOK_UTIL_H_
