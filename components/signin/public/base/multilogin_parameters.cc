// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/multilogin_parameters.h"

#include <sstream>
#include <string>

#include "base/strings/string_util.h"

namespace signin {

namespace {

std::string GetAccountsAsString(
    const std::vector<CoreAccountId>& accounts_to_send) {
  std::vector<std::string> parts;
  for (const CoreAccountId& account : accounts_to_send)
    parts.push_back(account.ToString());
  return "{" + base::JoinString(parts, ", ") + "}";
}

}  // namespace

MultiloginParameters::MultiloginParameters() = default;

MultiloginParameters::MultiloginParameters(
    const gaia::MultiloginMode mode,
    const std::vector<CoreAccountId>& accounts_to_send)
    : mode(mode), accounts_to_send(accounts_to_send) {}

MultiloginParameters::~MultiloginParameters() = default;

MultiloginParameters::MultiloginParameters(const MultiloginParameters& other) {
  mode = other.mode;
  accounts_to_send = other.accounts_to_send;
}

std::string MultiloginParameters::ToString() const {
  std::stringstream ss;

  ss << "mode: " << static_cast<int>(mode)
     << ", accounts_to_send: " << GetAccountsAsString(accounts_to_send);

  return ss.str();
}

MultiloginParameters& MultiloginParameters::operator=(
    const MultiloginParameters& other) {
  mode = other.mode;
  accounts_to_send = other.accounts_to_send;
  return *this;
}

std::ostream& operator<<(std::ostream& out, const MultiloginParameters& p) {
  return out << p.ToString();
}

}  // namespace signin
