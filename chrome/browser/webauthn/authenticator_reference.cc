// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/authenticator_reference.h"

#include <utility>

AuthenticatorReference::AuthenticatorReference(
    base::StringPiece authenticator_id,
    base::StringPiece16 authenticator_display_name,
    device::FidoTransportProtocol transport,
    bool is_in_pairing_mode)
    : authenticator_id_(authenticator_id),
      authenticator_display_name_(authenticator_display_name),
      transport_(transport),
      is_in_pairing_mode_(is_in_pairing_mode) {}

AuthenticatorReference::AuthenticatorReference(AuthenticatorReference&& data) =
    default;

AuthenticatorReference& AuthenticatorReference::operator=(
    AuthenticatorReference&& other) = default;

AuthenticatorReference::~AuthenticatorReference() = default;

void AuthenticatorReference::SetAuthenticatorId(std::string authenticator_id) {
  authenticator_id_ = std::move(authenticator_id);
}

void AuthenticatorReference::SetIsInPairingMode(bool is_in_pairing_mode) {
  is_in_pairing_mode_ = is_in_pairing_mode;
}

void AuthenticatorReference::SetDispatched(bool dispatched) {
  dispatched_ = dispatched;
}
