// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/mini_installer/enumerate_resources.h"

#include <windows.h>

#include "chrome/installer/mini_installer/memory_range.h"

namespace mini_installer {

namespace {

// Returns the memory range occupied by a resource in `module`. The returned
// range is only valid while `module` remains loaded in the process.
MemoryRange GetResourceRange(HMODULE module,
                             const wchar_t* name,
                             const wchar_t* type) {
  // Resource handles are not real HGLOBALs so do not attempt to close them.
  // Resources are freed when the containing module is unloaded.
  if (auto* resource = ::FindResource(module, name, type); resource) {
    if (auto size = ::SizeofResource(module, resource); size) {
      if (auto* handle = ::LoadResource(module, resource); handle) {
        if (const auto* data = ::LockResource(handle); data) {
          return {reinterpret_cast<const uint8_t*>(data), size};
        }
      }
    }
  }
  return {};
}

// Processes a resource of type `type` in `module` on behalf of a call to
// EnumResourceNames. On each call, `name` contains the name of a resource. A
// TRUE return value continues the enumeration, whereas FALSE stops it.
// static
BOOL CALLBACK EnumResNameProc(HMODULE module,
                              const wchar_t* type,
                              wchar_t* name,
                              LONG_PTR l_param) {
  if (!l_param) {
    return FALSE;  // Break: impossible condition.
  }

  if (IS_INTRESOURCE(name)) {
    return FALSE;  // Break: resources with integer names are unexpected.
  }

  const MemoryRange data_range = GetResourceRange(module, name, type);
  return (!data_range.empty() &&
          reinterpret_cast<ResourceEnumeratorDelegate*>(l_param)->OnResource(
              name, data_range))
             ? TRUE
             : FALSE;
}

}  // namespace

bool ResourceEnumeratorDelegate::OnResource(const wchar_t* name,
                                            const MemoryRange& data_range) {
  return false;
}

bool EnumerateResources(ResourceEnumeratorDelegate&& delegate,
                        HMODULE module,
                        const wchar_t* type) {
  return ::EnumResourceNames(module, type, &EnumResNameProc,
                             reinterpret_cast<LONG_PTR>(&delegate)) != FALSE;
}

}  // namespace mini_installer
