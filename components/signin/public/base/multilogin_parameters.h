// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_MULTILOGIN_PARAMETERS_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_MULTILOGIN_PARAMETERS_H_

#include <ostream>
#include <string>
#include <vector>

#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"

namespace signin {

struct MultiloginParameters {
  // Parameters with UPDATE mode and empty accounts.
  MultiloginParameters();
  MultiloginParameters(gaia::MultiloginMode mode,
                       const std::vector<CoreAccountId>& accounts_to_send);
  MultiloginParameters(const MultiloginParameters& other);
  MultiloginParameters& operator=(const MultiloginParameters& other);
  ~MultiloginParameters();

  std::string ToString() const;

  bool operator==(const MultiloginParameters& other) const {
    return mode == other.mode && accounts_to_send == other.accounts_to_send;
  }

  bool operator!=(const MultiloginParameters& other) const {
    return !(*this == other);
  }

  gaia::MultiloginMode mode =
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER;
  std::vector<CoreAccountId> accounts_to_send;
};

std::ostream& operator<<(std::ostream& out, const MultiloginParameters& p);

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_MULTILOGIN_PARAMETERS_H_
