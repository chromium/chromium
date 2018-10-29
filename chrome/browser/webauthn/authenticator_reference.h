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
class AuthenticatorReference {
 public:
  AuthenticatorReference(base::StringPiece device_id,
                         base::StringPiece16 authenticator_display_name,
                         device::FidoTransportProtocol transport,
                         bool is_in_pairing_mode);
  AuthenticatorReference(AuthenticatorReference&& data);
  AuthenticatorReference& operator=(AuthenticatorReference&& other);
  ~AuthenticatorReference();

  void SetAuthenticatorId(std::string authenticator_id);
  void SetIsInPairingMode(bool is_in_pairing_mode);
  void SetDispatched(bool dispatched);

  const std::string& authenticator_id() const { return authenticator_id_; }
  const base::string16& authenticator_display_name() const {
    return authenticator_display_name_;
  }
  device::FidoTransportProtocol transport() const { return transport_; }
  bool is_in_pairing_mode() const { return is_in_pairing_mode_; }
  bool dispatched() const { return dispatched_; }

 private:
  std::string authenticator_id_;
  base::string16 authenticator_display_name_;
  device::FidoTransportProtocol transport_;
  bool is_in_pairing_mode_ = false;
  bool dispatched_ = false;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorReference);
};

#endif  // CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REFERENCE_H_
