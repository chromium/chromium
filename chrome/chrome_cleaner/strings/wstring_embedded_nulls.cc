// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/strings/wstring_embedded_nulls.h"

namespace chrome_cleaner {

WStringEmbeddedNulls::WStringEmbeddedNulls() = default;

WStringEmbeddedNulls::WStringEmbeddedNulls(nullptr_t)
    : WStringEmbeddedNulls() {}

WStringEmbeddedNulls::WStringEmbeddedNulls(const WStringEmbeddedNulls& str) =
    default;

WStringEmbeddedNulls::WStringEmbeddedNulls(const wchar_t* const array,
                                           size_t size) {
  // Empty strings should always be represented as an empty array.
  if (array && size > 0)
    data_ = std::vector<wchar_t>(array, array + size);
}

WStringEmbeddedNulls::WStringEmbeddedNulls(const std::vector<wchar_t>& str)
    : WStringEmbeddedNulls(str.data(), str.size()) {}

WStringEmbeddedNulls::WStringEmbeddedNulls(const std::wstring& str)
    : WStringEmbeddedNulls(str.data(), str.size()) {}

WStringEmbeddedNulls::WStringEmbeddedNulls(base::WStringPiece str)
    : WStringEmbeddedNulls(str.data(), str.size()) {}

WStringEmbeddedNulls::WStringEmbeddedNulls(std::initializer_list<wchar_t> il)
    : data_(il) {}

WStringEmbeddedNulls::~WStringEmbeddedNulls() = default;

WStringEmbeddedNulls& WStringEmbeddedNulls::operator=(
    const WStringEmbeddedNulls& str) = default;

bool WStringEmbeddedNulls::operator==(const WStringEmbeddedNulls& str) const {
  return CastAsWStringPiece() == str.CastAsWStringPiece();
}

size_t WStringEmbeddedNulls::size() const {
  return data_.size();
}

const base::WStringPiece WStringEmbeddedNulls::CastAsWStringPiece() const {
  return base::WStringPiece(data_.data(), data_.size());
}

const wchar_t* WStringEmbeddedNulls::CastAsWCharArray() const {
  return data_.data();
}

const uint16_t* WStringEmbeddedNulls::CastAsUInt16Array() const {
  return reinterpret_cast<const uint16_t* const>(data_.data());
}

}  // namespace chrome_cleaner
