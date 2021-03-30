// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REFERENCE_H_
#define CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REFERENCE_H_

#include <string>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "device/fido/fido_transport_protocol.h"

// Encapsulates information about authenticators that have been found but to
// which the CTAP request is not dispatched until after receiving confirmation
// from the user via the UI.
struct AuthenticatorReference {
  AuthenticatorReference(base::StringPiece device_id,
                         device::FidoTransportProtocol transport);
  AuthenticatorReference(AuthenticatorReference&& data);
  AuthenticatorReference& operator=(AuthenticatorReference&& other);
  ~AuthenticatorReference();

  std::string authenticator_id;
  device::FidoTransportProtocol transport;
  bool dispatched = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(AuthenticatorReference);
};

#endif  // CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REFERENCE_H_
