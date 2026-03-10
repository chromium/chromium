// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/common/base64_utils.h"

#include <optional>
#include <string>

#include "base/base64.h"
#include "base/base64url.h"

namespace private_ai {

std::optional<std::string> ConvertBase64toBase64Url(
    const std::string& input,
    base::Base64UrlEncodePolicy encode_policy) {
  std::string decoded;
  if (!base::Base64UrlDecode(input, base::Base64UrlDecodePolicy::IGNORE_PADDING,
                             &decoded)) {
    return std::nullopt;
  }

  std::string encoded;
  base::Base64UrlEncode(decoded, encode_policy, &encoded);
  return encoded;
}

}  // namespace private_ai
