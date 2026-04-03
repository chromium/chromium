// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/token_binding_info.h"

#include <utility>
#include <vector>

namespace signin {

TokenBindingInfo::TokenBindingInfo() = default;

TokenBindingInfo::TokenBindingInfo(const TokenBindingInfo&) = default;
TokenBindingInfo& TokenBindingInfo::operator=(const TokenBindingInfo&) =
    default;
TokenBindingInfo::TokenBindingInfo(TokenBindingInfo&&) = default;
TokenBindingInfo& TokenBindingInfo::operator=(TokenBindingInfo&&) = default;

TokenBindingInfo::TokenBindingInfo(std::vector<uint8_t> wrapped_binding_key,
                                   bool mtls_token_binding)
    : wrapped_binding_key(std::move(wrapped_binding_key)),
      mtls_token_binding(mtls_token_binding) {}

TokenBindingInfo::~TokenBindingInfo() = default;

}  // namespace signin
