// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/demo_mode/utils/dimensions_utils.h"

#include "third_party/abseil-cpp/absl/strings/ascii.h"
#include "third_party/icu/source/common/unicode/bytestream.h"
#include "third_party/icu/source/common/unicode/casemap.h"

namespace ash::demo_mode {

std::string CanonicalizeDimension(const std::string& dimension_value) {
  std::string canonicalized_value;

  icu::StringByteSink<std::string> byte_sink(&canonicalized_value);
  UErrorCode error_code = U_ZERO_ERROR;
  icu::CaseMap::utf8Fold(/* options= */ 0, dimension_value, byte_sink,
                         /* edits= */ nullptr, error_code);
  canonicalized_value.erase(
      std::remove_if(canonicalized_value.begin(), canonicalized_value.end(),
                     [](unsigned char c) {
                       return absl::ascii_ispunct(c) || absl::ascii_isspace(c);
                     }),
      canonicalized_value.end());
  return canonicalized_value;
}

}  // namespace ash::demo_mode
