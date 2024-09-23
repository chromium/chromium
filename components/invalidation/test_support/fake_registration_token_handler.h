// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_TEST_SUPPORT_FAKE_REGISTRATION_TOKEN_HANDLER_H_
#define COMPONENTS_INVALIDATION_TEST_SUPPORT_FAKE_REGISTRATION_TOKEN_HANDLER_H_

#include <string>

#include "base/time/time.h"
#include "components/invalidation/invalidation_listener.h"

namespace invalidation {

class FakeRegistrationTokenHandler : public RegistrationTokenHandler {
 public:
  void OnRegistrationTokenReceived(const std::string& registration_token,
                                   base::Time token_end_of_live) override;

  const std::string& get_registration_token() const {
    return registration_token_;
  }

 private:
  std::string registration_token_;
  base::Time token_end_of_live_;
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_TEST_SUPPORT_FAKE_REGISTRATION_TOKEN_HANDLER_H_
