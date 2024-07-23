// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/connectors_service_base.h"

#include "components/prefs/pref_service.h"

namespace enterprise_connectors {

ConnectorsServiceBase::DmToken::DmToken(const std::string& value,
                                        policy::PolicyScope scope)
    : value(value), scope(scope) {}
ConnectorsServiceBase::DmToken::DmToken(DmToken&&) = default;
ConnectorsServiceBase::DmToken& ConnectorsServiceBase::DmToken::operator=(
    DmToken&&) = default;
ConnectorsServiceBase::DmToken::DmToken(const DmToken&) = default;
ConnectorsServiceBase::DmToken& ConnectorsServiceBase::DmToken::operator=(
    const DmToken&) = default;
ConnectorsServiceBase::DmToken::~DmToken() = default;

std::optional<std::string>
ConnectorsServiceBase::GetDMTokenForRealTimeUrlCheck() const {
  if (!ConnectorsEnabled()) {
    return std::nullopt;
  }

  if (GetPrefs()->GetInteger(
          prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckMode) ==
      safe_browsing::REAL_TIME_CHECK_DISABLED) {
    return std::nullopt;
  }

  std::optional<DmToken> dm_token =
      GetDmToken(prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckScope);

  if (dm_token.has_value()) {
    return dm_token.value().value;
  }
  return std::nullopt;
}

safe_browsing::EnterpriseRealTimeUrlCheckMode
ConnectorsServiceBase::GetAppliedRealTimeUrlCheck() const {
  if (!ConnectorsEnabled() ||
      !GetDmToken(prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckScope)
           .has_value()) {
    return safe_browsing::REAL_TIME_CHECK_DISABLED;
  }

  return static_cast<safe_browsing::EnterpriseRealTimeUrlCheckMode>(
      GetPrefs()->GetInteger(
          prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckMode));
}

}  // namespace enterprise_connectors
