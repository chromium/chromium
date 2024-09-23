// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/test_support/fake_registration_token_handler.h"

namespace invalidation {

void FakeRegistrationTokenHandler::OnRegistrationTokenReceived(
    const std::string& registration_token,
    base::Time token_end_of_live) {
  registration_token_ = registration_token;
  token_end_of_live_ = token_end_of_live;
}

}  // namespace invalidation
