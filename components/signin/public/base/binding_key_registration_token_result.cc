// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/binding_key_registration_token_result.h"

namespace signin {

BindingKeyRegistrationTokenResult::BindingKeyRegistrationTokenResult(
    unexportable_keys::UnexportableKeyId binding_key_id,
    std::vector<uint8_t> wrapped_binding_key,
    std::string registration_token)
    : binding_key_id(binding_key_id),
      wrapped_binding_key(std::move(wrapped_binding_key)),
      registration_token(std::move(registration_token)) {}

BindingKeyRegistrationTokenResult::BindingKeyRegistrationTokenResult(
    BindingKeyRegistrationTokenResult&&) = default;
BindingKeyRegistrationTokenResult& BindingKeyRegistrationTokenResult::operator=(
    BindingKeyRegistrationTokenResult&&) = default;

BindingKeyRegistrationTokenResult::~BindingKeyRegistrationTokenResult() =
    default;

}  // namespace signin
