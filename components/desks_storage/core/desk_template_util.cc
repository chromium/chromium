// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/desk_template_util.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/re2/src/re2/re2.h"

namespace {

// Duplicate value format.
constexpr char kDuplicateNumberFormat[] = "(%d)";
// Initial duplicate number value.
constexpr char kInitialDuplicateNumberValue[] = " (1)";
// Regex used in determining if duplicate name should be incremented.
constexpr char kDuplicateNumberRegex[] = "\\(([0-9]+)\\)$";

}  // namespace

namespace desks_storage {

namespace desk_template_util {

std::u16string AppendDuplicateNumberToDuplicateName(
    const std::u16string& duplicate_name_u16) {
  std::string duplicate_name = base::UTF16ToUTF8(duplicate_name_u16);
  int found_duplicate_number;

  if (RE2::PartialMatch(duplicate_name, kDuplicateNumberRegex,
                        &found_duplicate_number)) {
    RE2::Replace(
        &duplicate_name, kDuplicateNumberRegex,
        base::StringPrintf(kDuplicateNumberFormat, found_duplicate_number + 1));
  } else {
    duplicate_name.append(kInitialDuplicateNumberValue);
  }

  return base::UTF8ToUTF16(duplicate_name);
}

}  // namespace desk_template_util

}  // namespace desks_storage
