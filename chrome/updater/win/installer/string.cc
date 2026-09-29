// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/installer/string.h"

#include <windows.h>

#include <stddef.h>

#include "base/compiler_specific.h"

namespace updater {

size_t SafeStrLen(const wchar_t* str, size_t alloc_size) {
  if (!str || !alloc_size) {
    return 0;
  }
  size_t len = 0;
  while (--alloc_size && UNSAFE_TODO(str[len]) != L'\0') {
    ++len;
  }
  return len;
}

bool SafeStrCopy(wchar_t* dest, size_t dest_size, const wchar_t* src) {
  if (!dest || !dest_size) {
    return false;
  }

  wchar_t* write = dest;
  for (size_t remaining = dest_size; remaining != 0; --remaining) {
    if ((*UNSAFE_TODO(write++) = *UNSAFE_TODO(src++)) == L'\0') {
      return true;
    }
  }

  // If we fail, we do not want to leave the string with partially copied
  // contents.  The reason for this is that we use these strings mostly for
  // named objects such as files.  If we copy a partial name, then that could
  // match with something we do not want it to match with.
  // Furthermore, since SafeStrCopy is called from SafeStrCat, we do not
  // want to mutate the string in case the caller handles the error of a
  // failed concatenation.  For example:
  //
  // wchar_t buf[5] = {};
  // if (!SafeStrCat(buf, _countof(buf), kLongName))
  //   SafeStrCat(buf, _countof(buf), kShortName);
  //
  // If we were to return false in the first call to SafeStrCat but still
  // mutate the buffer, the buffer will be in an unexpected state.
  *dest = L'\0';
  return false;
}

// Safer replacement for lstrcat function.
bool SafeStrCat(wchar_t* dest, size_t dest_size, const wchar_t* src) {
  // Use SafeStrLen instead of lstrlen just in case the |dest| buffer isn't
  // terminated.
  size_t str_len = SafeStrLen(dest, dest_size);
  return SafeStrCopy(UNSAFE_TODO(dest + str_len), dest_size - str_len, src);
}

}  // namespace updater
