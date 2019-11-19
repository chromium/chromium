// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/authenticator_reference.h"

AuthenticatorReference::AuthenticatorReference(
    base::StringPiece authenticator_id,
    base::StringPiece16 authenticator_display_name,
    device::FidoTransportProtocol transport,
    bool is_in_pairing_mode,
    bool is_paired,
    bool requires_ble_pairing_pin)
    : authenticator_id(authenticator_id),
      authenticator_display_name(authenticator_display_name),
      transport(transport),
      is_in_pairing_mode(is_in_pairing_mode),
      is_paired(is_paired),
      requires_ble_pairing_pin(requires_ble_pairing_pin) {}

AuthenticatorReference::AuthenticatorReference(AuthenticatorReference&& data) =
    default;

AuthenticatorReference& AuthenticatorReference::operator=(
    AuthenticatorReference&& other) = default;

AuthenticatorReference::~AuthenticatorReference() = default;
