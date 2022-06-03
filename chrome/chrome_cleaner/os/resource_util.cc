// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/resource_util.h"

#include <windows.h>

#include <stdint.h>

#include "base/check_op.h"

namespace chrome_cleaner {

bool LoadResourceOfKind(uint32_t resource_id,
                        const wchar_t* kind,
                        base::StringPiece* output) {
  DCHECK(output);
  HRSRC handle = ::FindResource(::GetModuleHandle(nullptr),
                                MAKEINTRESOURCE(resource_id), kind);
  if (!handle)
    return false;

  HGLOBAL loaded_buffer = ::LoadResource(::GetModuleHandle(nullptr), handle);
  DPCHECK(loaded_buffer);
  LPVOID locked_buffer = ::LockResource(loaded_buffer);
  DPCHECK(locked_buffer);
  DWORD size = ::SizeofResource(::GetModuleHandle(nullptr), handle);
  DCHECK_GT(size, 0U);

  *output = base::StringPiece(reinterpret_cast<char*>(locked_buffer), size);
  return true;
}

}  // namespace chrome_cleaner
