// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/quick_start/types.h"

#include <cstdint>
#include <vector>

#include "base/base64.h"
#include "base/base64url.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::quick_start {

Base64UrlString Base64UrlEncode(const std::vector<uint8_t>& data) {
  Base64UrlString result;
  base::Base64UrlEncode(data, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &(result.value()));
  return result;
}

absl::optional<Base64UrlString> Base64UrlTranscode(const Base64String& data) {
  absl::optional<std::vector<uint8_t>> decoded_bytes =
      base::Base64Decode(*data);
  if (!decoded_bytes) {
    return absl::nullopt;
  }

  return Base64UrlEncode(*decoded_bytes);
}

}  // namespace ash::quick_start
