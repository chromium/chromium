// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/multilogin_parameters.h"

namespace signin {

MultiloginParameters::MultiloginParameters(
    const gaia::MultiloginMode mode,
    const std::vector<CoreAccountId>& accounts_to_send)
    : mode(mode), accounts_to_send(accounts_to_send) {}

MultiloginParameters::~MultiloginParameters() {}

MultiloginParameters::MultiloginParameters(const MultiloginParameters& other) {
  mode = other.mode;
  accounts_to_send = other.accounts_to_send;
}

MultiloginParameters& MultiloginParameters::operator=(
    const MultiloginParameters& other) {
  mode = other.mode;
  accounts_to_send = other.accounts_to_send;
  return *this;
}

}  // namespace signin
