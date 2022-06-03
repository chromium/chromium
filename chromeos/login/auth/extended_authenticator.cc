// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/auth/extended_authenticator.h"

#include "chromeos/login/auth/extended_authenticator_impl.h"

namespace chromeos {

// static
scoped_refptr<ExtendedAuthenticator> ExtendedAuthenticator::Create(
      AuthStatusConsumer* consumer) {
  return ExtendedAuthenticatorImpl::Create(consumer);
}

ExtendedAuthenticator::ExtendedAuthenticator() = default;

ExtendedAuthenticator::~ExtendedAuthenticator() = default;

}  // namespace chromeos
