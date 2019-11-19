// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hook_util.h"

#include <versionhelpers.h>  // windows.h must be before

#include "base/win/pe_image.h"
#include "chrome/chrome_elf/nt_registry/nt_registry.h"  // utils
#include "sandbox/win/src/interception_internal.h"
#include "sandbox/win/src/internal_types.h"
#include "sandbox/win/src/sandbox_utils.h"
#include "sandbox/win/src/service_resolver.h"

namespace {

//------------------------------------------------------------------------------
// Common hooking utility functions - LOCAL
//------------------------------------------------------------------------------

// Change the page protections to writable, copy the data,
// restore protections. Returns a winerror code.
DWORD PatchMem(void* target, void* new_bytes, size_t length) {
  if (target == nullptr || new_bytes == nullptr || length == 0)
    return ERROR_INVALID_PARAMETER;

  // Preserve executable state.
  MEMORY_BASIC_INFORMATION memory_info = {};
  if (!::VirtualQuery(target, &memory_info, sizeof(memory_info))) {
    return GetLastError();
  }

  DWORD is_executable = (PAGE_EXECUTE | PAGE_EXECUTE_READ |
                         PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY) &
                        memory_info.Protect;

  // Make target writeable.
  DWORD old_page_protection = 0;
  if (!::VirtualProtect(target, length,
                        is_executable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE,
                        &old_page_protection)) {
    return GetLastError();
  }

  // Write the data.
  ::memcpy(target, new_bytes, length);

  // Restore old page protection.
  if (!::VirtualProtect(target, length, old_page_protection,
                        &old_page_protection)) {
// Yes, this could fail.  However, memory was already patched.
#ifdef _DEBUG
    assert(false);
#endif  // _DEBUG
  }

  return NO_ERROR;
}

//------------------------------------------------------------------------------
// Import Address Table hooking support - LOCAL
//------------------------------------------------------------------------------

void* GetIATFunctionPtr(IMAGE_THUNK_DATA* iat_thunk) {
  if (iat_thunk == nullptr)
    return nullptr;

  // Works around the 64 bit portability warning:
  // The Function member inside IMAGE_THUNK_DATA is really a pointer
  // to the IAT function. IMAGE_THUNK_DATA correctly maps to IMAGE_THUNK_DATA32
  // or IMAGE_THUNK_DATA64 for correct pointer size.
  union FunctionThunk {
    IMAGE_THUNK_DATA thunk;
    void* pointer;
  } iat_function;

  iat_function.thunk = *iat_thunk;
  return iat_function.pointer;
}

// Used to pass target function information during pe_image enumeration.
struct IATHookFunctionInfo {
  bool finished_operation;
  const char* imported_from_module;
  const char* function_name;
  void* new_function;
  void** old_function;
  IMAGE_THUNK_DATA** iat_thunk;
  DWORD return_code;
};

// Callback function for pe_image enumeration.  This function is called from
// within PEImage::EnumOneImportChunk().
// NOTE: Returning true means continue enumerating.  False means stop.
bool IATFindHookFuncCallback(const base::win::PEImage& image,
                             const char* module,
                             DWORD ordinal,
                             const char* import_name,
                             DWORD hint,
                             IMAGE_THUNK_DATA* iat,
                             void* cookie) {
  IATHookFunctionInfo* hook_func_info =
      reinterpret_cast<IATHookFunctionInfo*>(cookie);
  if (hook_func_info == nullptr)
    return false;

  // Check for the right function.
  if (import_name == nullptr ||
      ::strnicmp(import_name, hook_func_info->function_name,
                 ::strlen(import_name)) != 0)
    return true;

  // At this point, the target function was found.  Even if something fails now,
  // don't do any further enumerating.
  hook_func_info->finished_operation = true;

  // This is it.  Do the hook!
  // 1) Save the old function pointer.
  *(hook_func_info->old_function) = GetIATFunctionPtr(iat);

  // 2) Save the IAT thunk.
  *(hook_func_info->iat_thunk) = iat;

  // 3) Sanity check the pointer sizes (architectures).
  if (sizeof(iat->u1.Function) != sizeof(hook_func_info->new_function)) {
    hook_func_info->return_code = ERROR_BAD_ENVIRONMENT;
#ifdef _DEBUG
    assert(false);
#endif  // _DEBUG
    return false;
  }

  // 4) Sanity check that the new hook function is not actually the
  //    same as the existing function for this import!
  if (*(hook_func_info->old_function) == hook_func_info->new_function) {
    hook_func_info->return_code = ERROR_INVALID_FUNCTION;
#ifdef _DEBUG
    assert(false);
#endif  // _DEBUG
    return false;
  }

  // 5) Patch the function pointer.
  hook_func_info->return_code =
      PatchMem(&(iat->u1.Function), &(hook_func_info->new_function),
               sizeof(hook_func_info->new_function));

  return false;
}

// Applies an import-address-table hook.  Returns a system winerror.h code.
// Call RemoveIATHook() with |new_function|, |old_function| and |iat_thunk|
// to remove the hook.
DWORD ApplyIATHook(HMODULE module_handle,
                   const char* imported_from_module,
                   const char* function_name,
                   void* new_function,
                   void** old_function,
                   IMAGE_THUNK_DATA** iat_thunk) {
  base::win::PEImage target_image(module_handle);
  if (!target_image.VerifyMagic())
    return ERROR_INVALID_PARAMETER;

  IATHookFunctionInfo hook_info = {false,
                                   imported_from_module,
                                   function_name,
                                   new_function,
                                   old_function,
                                   iat_thunk,
                                   ERROR_PROC_NOT_FOUND};

  // First go through the IAT. If we don't find the import we are looking
  // for in IAT, search delay import table.
  target_image.EnumAllImports(IATFindHookFuncCallback, &hook_info,
                              imported_from_module);
  if (!hook_info.finished_operation) {
    target_image.EnumAllDelayImports(IATFindHookFuncCallback, &hook_info,
                                     imported_from_module);
  }

  return hook_info.return_code;
}

// Removes an import-address-table hook.  Returns a system winerror.h code.
DWORD RemoveIATHook(void* intercept_function,
                    void* original_function,
                    IMAGE_THUNK_DATA* iat_thunk) {
  if (GetIATFunctionPtr(iat_thunk) != intercept_function)
    // Someone else has messed with the same target. Cannot unpatch.
    return ERROR_INVALID_FUNCTION;

  return PatchMem(&(iat_thunk->u1.Function), &original_function,
                  sizeof(original_function));
}

}  // namespace

namespace elf_hook {

//------------------------------------------------------------------------------
// System Service hooking support
//------------------------------------------------------------------------------

sandbox::ServiceResolverThunk* HookSystemService(bool relaxed) {
  // Create a thunk via the appropriate ServiceResolver instance.
  sandbox::ServiceResolverThunk* thunk = nullptr;

  // No hooking on unsupported OS versions.
  if (!::IsWindows7OrGreater())
    return thunk;

  // Pseudo-handle, no need to close.
  HANDLE current_process = ::GetCurrentProcess();

#if defined(_WIN64)
  // ServiceResolverThunk can handle all the formats in 64-bit (instead only
  // handling one like it does in 32-bit versions).
  thunk = new sandbox::ServiceResolverThunk(current_process, relaxed);
#else
  if (nt::IsCurrentProcWow64()) {
    if (::IsWindows10OrGreater())
      thunk = new sandbox::Wow64W10ResolverThunk(current_process, relaxed);
    else if (::IsWindows8OrGreater())
      thunk = new sandbox::Wow64W8ResolverThunk(current_process, relaxed);
    else
      thunk = new sandbox::Wow64ResolverThunk(current_process, relaxed);
  } else if (::IsWindows8OrGreater()) {
    thunk = new sandbox::Win8ResolverThunk(current_process, relaxed);
  } else {
    thunk = new sandbox::ServiceResolverThunk(current_process, relaxed);
  }
#endif

  return thunk;
}

//------------------------------------------------------------------------------
// Import Address Table hooking support
//------------------------------------------------------------------------------

IATHook::IATHook()
    : intercept_function_(nullptr),
      original_function_(nullptr),
      iat_thunk_(nullptr) {}

IATHook::~IATHook() {
  if (intercept_function_ != nullptr) {
    if (Unhook() != NO_ERROR) {
#ifdef _DEBUG
      assert(false);
#endif  // _DEBUG
    }
  }
}

DWORD IATHook::Hook(HMODULE module,
                    const char* imported_from_module,
                    const char* function_name,
                    void* new_function) {
  if ((module == 0 || module == INVALID_HANDLE_VALUE) ||
      imported_from_module == nullptr || function_name == nullptr ||
      new_function == nullptr)
    return ERROR_INVALID_PARAMETER;

  // Only hook once per object, to ensure unhook.
  if (intercept_function_ != nullptr || original_function_ != nullptr ||
      iat_thunk_ != nullptr)
    return ERROR_SHARING_VIOLATION;

  DWORD winerror = ApplyIATHook(module, imported_from_module, function_name,
                                new_function, &original_function_, &iat_thunk_);
  if (winerror == NO_ERROR)
    intercept_function_ = new_function;

  return winerror;
}

DWORD IATHook::Unhook() {
  if (intercept_function_ == nullptr || original_function_ == nullptr ||
      iat_thunk_ == nullptr)
    return ERROR_INVALID_PARAMETER;

  DWORD winerror =
      RemoveIATHook(intercept_function_, original_function_, iat_thunk_);

  intercept_function_ = nullptr;
  original_function_ = nullptr;
  iat_thunk_ = nullptr;

  return winerror;
}

}  // namespace elf_hook
