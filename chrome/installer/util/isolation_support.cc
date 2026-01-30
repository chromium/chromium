// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/isolation_support.h"

#include <string>

#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/install_static/install_modes.h"

namespace {

template <std::size_t N>
consteval bool wide_is_ascii(const wchar_t (&s)[N]) {
  for (const auto c : s) {
    if (c == L'\0') {
      break;
    }
    if (c > 0x7F) {
      return false;
    }
  }
  return true;
}

static_assert(wide_is_ascii(install_static::kCompanyPathName),
              "kCompanyPathName must be ASCII.");
static_assert(wide_is_ascii(install_static::kProductPathName),
              "kProductPathName must be ASCII.");

}  // namespace

namespace installer {

std::wstring GetIsolationAttributeName() {
  return base::ToUpperASCII(base::StrCat(
      {base::JoinString(
           base::SplitString(base::StrCat({install_static::kCompanyPathName,
                                           install_static::kProductPathName}),
                             L" ", base::WhitespaceHandling::TRIM_WHITESPACE,
                             base::SplitResult::SPLIT_WANT_NONEMPTY),
           L""),
       L"://ISOLATION"}));
}

}  // namespace installer
