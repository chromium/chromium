// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/connectors_service_base.h"

#include "components/enterprise/connectors/core/connectors_prefs.h"
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

  if (GetPrefs()->GetInteger(kEnterpriseRealTimeUrlCheckMode) ==
      REAL_TIME_CHECK_DISABLED) {
    return std::nullopt;
  }

  std::optional<DmToken> dm_token =
      GetDmToken(kEnterpriseRealTimeUrlCheckScope);

  if (dm_token.has_value()) {
    return dm_token.value().value;
  }
  return std::nullopt;
}

EnterpriseRealTimeUrlCheckMode
ConnectorsServiceBase::GetAppliedRealTimeUrlCheck() const {
  if (!ConnectorsEnabled() ||
      !GetDmToken(kEnterpriseRealTimeUrlCheckScope).has_value()) {
    return REAL_TIME_CHECK_DISABLED;
  }

  return static_cast<EnterpriseRealTimeUrlCheckMode>(
      GetPrefs()->GetInteger(kEnterpriseRealTimeUrlCheckMode));
}

}  // namespace enterprise_connectors
