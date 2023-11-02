// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_STRINGS_WSTRING_EMBEDDED_NULLS_H_
#define CHROME_CHROME_CLEANER_STRINGS_WSTRING_EMBEDDED_NULLS_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/strings/string_piece.h"

namespace chrome_cleaner {

// Holds a std::wstring with embedded nulls, which is allowed in registry paths.
// Provides a method to access the string as a WStringPiece, in order to prevent
// potential errors due to accidental uses of c_str().
class WStringEmbeddedNulls {
 public:
  typedef const wchar_t* const_iterator;

  WStringEmbeddedNulls();
  explicit WStringEmbeddedNulls(nullptr_t);
  WStringEmbeddedNulls(const WStringEmbeddedNulls& str);
  WStringEmbeddedNulls(const wchar_t* array, size_t size);
  explicit WStringEmbeddedNulls(const std::vector<wchar_t>& str);
  explicit WStringEmbeddedNulls(const std::wstring& str);
  explicit WStringEmbeddedNulls(base::WStringPiece str);
  explicit WStringEmbeddedNulls(std::initializer_list<wchar_t> chars);

  ~WStringEmbeddedNulls();

  WStringEmbeddedNulls& operator=(const WStringEmbeddedNulls& str);
  bool operator==(const WStringEmbeddedNulls& str) const;

  size_t size() const;

  // These methods don't create a copy of the underlying string. Make sure the
  // returned values don't outlive the current object.
  const base::WStringPiece CastAsWStringPiece() const;
  const wchar_t* CastAsWCharArray() const;
  const uint16_t* CastAsUInt16Array() const;

  const std::vector<wchar_t>& data() const { return data_; }
  std::vector<wchar_t>& data() { return data_; }

 private:
  std::vector<wchar_t> data_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_STRINGS_WSTRING_EMBEDDED_NULLS_H_
