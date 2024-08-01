// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/installer/mini_installer/mini_string.h"

#include <windows.h>

namespace {

// Returns true if the given two ASCII characters are same (ignoring case).
bool EqualASCIICharI(wchar_t a, wchar_t b) {
  if (a >= L'A' && a <= L'Z')
    a += (L'a' - L'A');
  if (b >= L'A' && b <= L'Z')
    b += (L'a' - L'A');
  return (a == b);
}

}  // namespace

namespace mini_installer {

// Formats a sequence of |bytes| as hex.  The |str| buffer must have room for
// at least 2*|size| + 1.
bool HexEncode(const void* bytes, size_t size, wchar_t* str, size_t str_size) {
  if (str_size <= (size * 2))
    return false;

  static const wchar_t kHexChars[] = L"0123456789ABCDEF";

  str[size * 2] = L'\0';

  for (size_t i = 0; i < size; ++i) {
    char b = reinterpret_cast<const char*>(bytes)[i];
    str[(i * 2)] = kHexChars[(b >> 4) & 0xf];
    str[(i * 2) + 1] = kHexChars[b & 0xf];
  }

  return true;
}

size_t SafeStrLen(const wchar_t* str, size_t alloc_size) {
  if (!str || !alloc_size)
    return 0;
  size_t len = 0;
  while (--alloc_size && str[len] != L'\0')
    ++len;
  return len;
}

bool SafeStrCopy(wchar_t* dest, size_t dest_size, const wchar_t* src) {
  if (!dest || !dest_size)
    return false;

  wchar_t* write = dest;
  for (size_t remaining = dest_size; remaining != 0; --remaining) {
    if ((*write++ = *src++) == L'\0')
      return true;
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

bool StrEndsWith(const wchar_t* str, const wchar_t* end_str) {
  if (str == nullptr || end_str == nullptr)
    return false;

  for (int i = lstrlen(str) - 1, j = lstrlen(end_str) - 1; j >= 0; --i, --j) {
    if (i < 0 || !EqualASCIICharI(str[i], end_str[j]))
      return false;
  }

  return true;
}

bool StrStartsWith(const wchar_t* str, const wchar_t* start_str) {
  if (str == nullptr || start_str == nullptr)
    return false;

  for (int i = 0; start_str[i] != L'\0'; ++i) {
    if (!EqualASCIICharI(str[i], start_str[i]))
      return false;
  }

  return true;
}

const wchar_t* SearchStringI(const wchar_t* source, const wchar_t* find) {
  if (!find || find[0] == L'\0')
    return source;

  const wchar_t* scan = source;
  while (*scan) {
    const wchar_t* s = scan;
    const wchar_t* f = find;

    while (*s && *f && EqualASCIICharI(*s, *f))
      ++s, ++f;

    if (!*f)
      return scan;

    ++scan;
  }

  return nullptr;
}

bool FindTagInStr(const wchar_t* str,
                  const wchar_t* tag,
                  const wchar_t** position) {
  int tag_length = ::lstrlen(tag);
  const wchar_t* scan = str;
  for (const wchar_t* tag_start = SearchStringI(scan, tag);
       tag_start != nullptr; tag_start = SearchStringI(scan, tag)) {
    scan = tag_start + tag_length;
    if (*scan == L'-' || *scan == L'\0') {
      if (position != nullptr)
        *position = tag_start;
      return true;
    }
  }
  return false;
}

const wchar_t* GetNameFromPathExt(const wchar_t* path, size_t size) {
  if (!size)
    return path;

  const wchar_t* current = &path[size - 1];
  while (current != path && L'\\' != *current)
    --current;

  // If no path separator found, just return |path|.
  // Otherwise, return a pointer right after the separator.
  return ((current == path) && (L'\\' != *current)) ? current : (current + 1);
}

wchar_t* GetNameFromPathExt(wchar_t* path, size_t size) {
  return const_cast<wchar_t*>(
      GetNameFromPathExt(const_cast<const wchar_t*>(path), size));
}

}  // namespace mini_installer
