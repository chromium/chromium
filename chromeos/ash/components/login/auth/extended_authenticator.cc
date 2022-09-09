// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/extended_authenticator.h"

#include "chromeos/ash/components/login/auth/extended_authenticator_impl.h"

namespace ash {

// static
scoped_refptr<ExtendedAuthenticator> ExtendedAuthenticator::Create(
    AuthStatusConsumer* consumer) {
  return ExtendedAuthenticatorImpl::Create(consumer);
}

ExtendedAuthenticator::ExtendedAuthenticator() = default;

ExtendedAuthenticator::~ExtendedAuthenticator() = default;

}  // namespace ash
