// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_BINDING_KEY_REGISTRATION_TOKEN_RESULT_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_BINDING_KEY_REGISTRATION_TOKEN_RESULT_H_

#include <cstdint>
#include <string>
#include <vector>

#include "components/unexportable_keys/unexportable_key_id.h"

namespace signin {

// A struct containing the necessary pieces of data for binding a refresh token
// to a token binding key.
//
// TODO(crbug.com/516196445): move this struct into
// `//components/signin/public/identity_manager/` once
// `binding_key_registration_token_helper.h` can also be moved.
struct BindingKeyRegistrationTokenResult {
  unexportable_keys::UnexportableKeyId binding_key_id;
  std::vector<uint8_t> wrapped_binding_key;
  std::string registration_token;

  BindingKeyRegistrationTokenResult(
      unexportable_keys::UnexportableKeyId binding_key_id,
      std::vector<uint8_t> wrapped_binding_key,
      std::string registration_token);

  BindingKeyRegistrationTokenResult(const BindingKeyRegistrationTokenResult&) =
      delete;
  BindingKeyRegistrationTokenResult& operator=(
      const BindingKeyRegistrationTokenResult&) = delete;
  BindingKeyRegistrationTokenResult(BindingKeyRegistrationTokenResult&&);
  BindingKeyRegistrationTokenResult& operator=(
      BindingKeyRegistrationTokenResult&&);

  ~BindingKeyRegistrationTokenResult();
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_BINDING_KEY_REGISTRATION_TOKEN_RESULT_H_
