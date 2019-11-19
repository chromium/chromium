// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_INSTALLER_STRING_H_
#define CHROME_UPDATER_WIN_INSTALLER_STRING_H_

#include <stddef.h>

namespace updater {

// NOTE: Do not assume that these string functions support UTF encoding.
// This is fine for the purposes of the mini_installer, but you have
// been warned!

// Formats a sequence of |bytes| as hex.  The |str| buffer must have room for
// at least 2*|size| + 1.
bool HexEncode(const void* bytes, size_t size, wchar_t* str, size_t str_size);

// Counts the number of characters in the string up to a maximum of
// alloc_size.  The highest return value from this function can therefore be
// alloc_size - 1 since |alloc_size| includes the \0 terminator.
size_t SafeStrLen(const wchar_t* str, size_t alloc_size);

// Simple replacement for CRT string copy method that does not overflow.
// Returns true if the source was copied successfully otherwise returns false.
// Parameter src is assumed to be nullptr terminated and the nullptr character
// is copied over to string dest.
bool SafeStrCopy(wchar_t* dest, size_t dest_size, const wchar_t* src);

// Simple replacement for CRT string copy method that does not overflow.
// Returns true if the source was copied successfully otherwise returns false.
// Parameter src is assumed to be nullptr terminated and the nullptr character
// is copied over to string dest.  If the return value is false, the |dest|
// string should be the same as it was before.
bool SafeStrCat(wchar_t* dest, size_t dest_size, const wchar_t* src);

// Function to check if a string (specified by str) starts with another string
// (specified by start_str). The comparison is string insensitive.
bool StrStartsWith(const wchar_t* str, const wchar_t* start_str);

// Takes the path to file and returns a pointer to the basename component.
// Example input -> output:
//     c:\full\path\to\file.ext -> file.ext
//     file.ext -> file.ext
// Note: |size| is the number of characters in |path| not including the string
// terminator.
const wchar_t* GetNameFromPathExt(const wchar_t* path, size_t size);
wchar_t* GetNameFromPathExt(wchar_t* path, size_t size);

// A string class that manages a fixed size buffer on the stack.
// The methods in the class are based on the above string methods and the
// class additionally is careful about proper buffer termination.
template <size_t kCapacity>
class StackString {
 public:
  StackString() {
    static_assert(kCapacity != 0, "invalid buffer size");
    buffer_[kCapacity] = L'\0';  // We always reserve 1 more than asked for.
    clear();
  }

  // We do not expose a constructor that accepts a string pointer on purpose.
  // We expect the caller to call assign() and handle failures.

  // Returns the number of reserved characters in this buffer, _including_
  // the reserved char for the terminator.
  size_t capacity() const { return kCapacity; }

  wchar_t* get() { return buffer_; }

  bool assign(const wchar_t* str) {
    return SafeStrCopy(buffer_, kCapacity, str);
  }

  bool append(const wchar_t* str) {
    return SafeStrCat(buffer_, kCapacity, str);
  }

  void clear() { buffer_[0] = L'\0'; }

  size_t length() const { return SafeStrLen(buffer_, kCapacity); }

  // Does a case insensitive search for a substring.
  const wchar_t* findi(const wchar_t* find) const {
    return SearchStringI(buffer_, find);
  }

  // Case insensitive string compare.
  int comparei(const wchar_t* str) const { return lstrcmpiW(buffer_, str); }

  // Case sensitive string compare.
  int compare(const wchar_t* str) const { return lstrcmpW(buffer_, str); }

  // Terminates the string at the specified location.
  // Note: this method has no effect if this object's length is less than
  // |location|.
  bool truncate_at(size_t location) {
    if (location >= kCapacity)
      return false;
    buffer_[location] = L'\0';
    return true;
  }

 protected:
  // We reserve 1 more than what is asked for as a safeguard against
  // off-by-one errors.
  wchar_t buffer_[kCapacity + 1];

 private:
  StackString(const StackString&);
  StackString& operator=(const StackString&);
};

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_INSTALLER_STRING_H_
