// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_MULTILOGIN_PARAMETERS_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_MULTILOGIN_PARAMETERS_H_

#include <string>
#include <vector>

#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"

namespace signin {

struct MultiloginParameters {
  MultiloginParameters(gaia::MultiloginMode mode,
                       const std::vector<CoreAccountId>& accounts_to_send);
  MultiloginParameters(const MultiloginParameters& other);
  MultiloginParameters& operator=(const MultiloginParameters& other);
  ~MultiloginParameters();

  // Needed for testing.
  bool operator==(const MultiloginParameters& other) const {
    return mode == other.mode && accounts_to_send == other.accounts_to_send;
  }

  gaia::MultiloginMode mode;
  std::vector<CoreAccountId> accounts_to_send;
};
}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_MULTILOGIN_PARAMETERS_H_
