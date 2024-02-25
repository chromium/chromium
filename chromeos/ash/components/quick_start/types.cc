// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/quick_start/types.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "base/base64.h"
#include "base/base64url.h"

namespace ash::quick_start {

Base64UrlString Base64UrlEncode(const std::vector<uint8_t>& data) {
  Base64UrlString result;
  base::Base64UrlEncode(data, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &(result.value()));
  return result;
}

Base64UrlString Base64UrlEncode(const std::string& data) {
  return Base64UrlEncode(std::vector<uint8_t>(data.begin(), data.end()));
}

std::optional<Base64UrlString> Base64UrlTranscode(const Base64String& data) {
  std::optional<std::vector<uint8_t>> decoded_bytes = base::Base64Decode(*data);
  if (!decoded_bytes) {
    return std::nullopt;
  }

  return Base64UrlEncode(*decoded_bytes);
}

}  // namespace ash::quick_start
