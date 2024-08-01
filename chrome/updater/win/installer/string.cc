// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/updater/win/installer/string.h"

#include <windows.h>

#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"

namespace {

// Returns true if the given two ASCII characters are same (ignoring case).
bool EqualASCIICharI(wchar_t a, wchar_t b) {
  if (a >= L'A' && a <= L'Z') {
    a += (L'a' - L'A');
  }
  if (b >= L'A' && b <= L'Z') {
    b += (L'a' - L'A');
  }
  return (a == b);
}

}  // namespace

namespace updater {

// Formats a sequence of |bytes| as hex.  The |str| buffer must have room for
// at least 2*|size| + 1.
bool HexEncode(const void* bytes, size_t size, wchar_t* str, size_t str_size) {
  if (str_size <= (size * 2)) {
    return false;
  }

  base::ranges::copy(base::HexEncode(bytes, size), str);
  str[size * 2] = L'\0';
  return true;
}

size_t SafeStrLen(const wchar_t* str, size_t alloc_size) {
  if (!str || !alloc_size) {
    return 0;
  }
  size_t len = 0;
  while (--alloc_size && str[len] != L'\0') {
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
    if ((*write++ = *src++) == L'\0') {
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
  // wchar_t buf[5] = {0};
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
  return SafeStrCopy(dest + str_len, dest_size - str_len, src);
}

bool StrStartsWith(const wchar_t* str, const wchar_t* start_str) {
  if (str == nullptr || start_str == nullptr) {
    return false;
  }

  for (int i = 0; start_str[i] != L'\0'; ++i) {
    if (!EqualASCIICharI(str[i], start_str[i])) {
      return false;
    }
  }

  return true;
}

const wchar_t* GetNameFromPathExt(const wchar_t* path, size_t size) {
  if (!size) {
    return path;
  }

  const wchar_t* current = &path[size - 1];
  while (current != path && L'\\' != *current) {
    --current;
  }

  // If no path separator found, just return |path|.
  // Otherwise, return a pointer right after the separator.
  return ((current == path) && (L'\\' != *current)) ? current : (current + 1);
}

wchar_t* GetNameFromPathExt(wchar_t* path, size_t size) {
  return const_cast<wchar_t*>(
      GetNameFromPathExt(const_cast<const wchar_t*>(path), size));
}

}  // namespace updater
