// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REFERENCE_H_
#define CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REFERENCE_H_

#include <string>
#include <string_view>

#include "device/fido/fido_transport_protocol.h"

// Encapsulates information about authenticators that have been found but to
// which the CTAP request is not dispatched until after receiving confirmation
// from the user via the UI.
struct AuthenticatorReference {
  AuthenticatorReference(std::string_view device_id,
                         device::FidoTransportProtocol transport,
                         device::AuthenticatorType type);

  AuthenticatorReference(const AuthenticatorReference&) = delete;
  AuthenticatorReference& operator=(const AuthenticatorReference&) = delete;

  AuthenticatorReference(AuthenticatorReference&& data);
  AuthenticatorReference& operator=(AuthenticatorReference&& other);

  ~AuthenticatorReference();

  std::string authenticator_id;
  // transport does not always match the transport returned by the original
  // `FidoAuthenticator`. Specifically, for authenticators that don't have a
  // transport, like the webauthn.dll authenticator, a transport of `kInternal`
  // may be synthesized to make other logic easier.
  device::FidoTransportProtocol transport;
  device::AuthenticatorType type;
  bool dispatched = false;
};

#endif  // CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REFERENCE_H_
