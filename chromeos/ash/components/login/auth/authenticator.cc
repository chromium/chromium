// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/authenticator.h"

namespace ash {

class AuthStatusConsumer;

Authenticator::Authenticator(AuthStatusConsumer* consumer)
    : consumer_(consumer) {}

Authenticator::~Authenticator() = default;

void Authenticator::SetConsumer(AuthStatusConsumer* consumer) {
  consumer_ = consumer;
}

}  // namespace ash
