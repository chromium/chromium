// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REFERENCE_H_
#define CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REFERENCE_H_

#include <string>

#include "base/strings/string_piece.h"
#include "device/fido/fido_transport_protocol.h"

// Encapsulates information about authenticators that have been found but to
// which the CTAP request is not dispatched until after receiving confirmation
// from the user via the UI.
struct AuthenticatorReference {
  AuthenticatorReference(base::StringPiece device_id,
                         device::FidoTransportProtocol transport);

  AuthenticatorReference(const AuthenticatorReference&) = delete;
  AuthenticatorReference& operator=(const AuthenticatorReference&) = delete;

  AuthenticatorReference(AuthenticatorReference&& data);
  AuthenticatorReference& operator=(AuthenticatorReference&& other);

  ~AuthenticatorReference();

  std::string authenticator_id;
  device::FidoTransportProtocol transport;
  bool dispatched = false;
};

#endif  // CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REFERENCE_H_
