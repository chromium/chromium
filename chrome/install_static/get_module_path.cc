// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <stddef.h>

#include <limits>

// http://blogs.msdn.com/oldnewthing/archive/2004/10/25/247180.aspx
extern "C" IMAGE_DOS_HEADER __ImageBase;

// Populates `buffer` (of `buffer_size`) with the path to the primary module,
// returning the length of the path excluding the terminator or zero on error.
// This should be linked into the primary module (i.e., chrome_elf) via the
// "primary_module" source set and exported for use by other modules in the
// process.
extern "C" size_t GetModulePath(wchar_t* buffer, size_t buffer_size) {
  if (buffer_size > std::numeric_limits<DWORD>::max()) {
    return 0;
  }
  DWORD path_len = ::GetModuleFileName(reinterpret_cast<HMODULE>(&__ImageBase),
                                       buffer, static_cast<DWORD>(buffer_size));
  if (path_len == 0 || path_len >= buffer_size) {
    return 0;  // Error or buffer too small.
  }

  return path_len;
}
