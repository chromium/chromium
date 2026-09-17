// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/consent_kit/consent_kit_result_parser.h"

#include <algorithm>
#include <utility>

namespace drive {

bool IsExpectedConsentFlow(const identity_consent::PrivacyFlowResult& result,
                           int expected_flow_id) {
  return result.has_flow_id() &&
         std::to_underlying(result.flow_id()) == expected_flow_id;
}

bool HasGrantedDriveConsent(const identity_consent::PrivacyFlowResult& result) {
  using ::identity_consent::ConsentSettingId;
  using ::identity_consent::Decision;

  return std::ranges::any_of(result.decision(), [](const auto& decision) {
    const bool is_workspace_setting =
        decision.ftc_consent_setting_id() ==
        ConsentSettingId::PERSONAL_CONTEXT_SEARCH_USING_WORKSPACE;

    const bool is_consented =
        decision.decision() == Decision::DECISION_CONSENT ||
        decision.decision() == Decision::DECISION_KEEP_CONSENT;

    return is_workspace_setting && is_consented;
  });
}

}  // namespace drive
