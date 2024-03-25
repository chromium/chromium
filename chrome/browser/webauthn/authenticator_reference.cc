// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/authenticator_reference.h"

#include <string_view>

AuthenticatorReference::AuthenticatorReference(
    std::string_view authenticator_id,
    device::FidoTransportProtocol transport,
    device::AuthenticatorType type)
    : authenticator_id(authenticator_id), transport(transport), type(type) {}

AuthenticatorReference::AuthenticatorReference(AuthenticatorReference&& data) =
    default;

AuthenticatorReference& AuthenticatorReference::operator=(
    AuthenticatorReference&& other) = default;

AuthenticatorReference::~AuthenticatorReference() = default;
