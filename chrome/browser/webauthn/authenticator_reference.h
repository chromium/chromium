// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REFERENCE_H_
#define CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REFERENCE_H_

#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "device/fido/fido_transport_protocol.h"

// Encapsulates information about authenticators that have been found but to
// which the CTAP request is not dispatched until after receiving confirmation
// from the user via the UI.
struct AuthenticatorReference {
  AuthenticatorReference(base::StringPiece device_id,
                         base::StringPiece16 authenticator_display_name,
                         device::FidoTransportProtocol transport,
                         bool is_in_pairing_mode,
                         bool is_paired,
                         bool requires_ble_pairing_pin);
  AuthenticatorReference(AuthenticatorReference&& data);
  AuthenticatorReference& operator=(AuthenticatorReference&& other);
  ~AuthenticatorReference();

  std::string authenticator_id;
  base::string16 authenticator_display_name;
  device::FidoTransportProtocol transport;
  bool is_in_pairing_mode = false;
  bool is_paired = false;
  bool requires_ble_pairing_pin = true;
  bool dispatched = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(AuthenticatorReference);
};

#endif  // CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REFERENCE_H_
