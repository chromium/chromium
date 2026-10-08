// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/decision_source.h"

#include <string>

namespace origin_gating {

std::string DecisionSourceToString(DecisionSource source) {
  switch (source) {
    case DecisionSource::kAllowSameOrigin:
      return "AllowSameOrigin";
    case DecisionSource::kAllowHttpLocalhost:
      return "AllowHttpLocalhost";
    case DecisionSource::kAllowAboutBlank:
      return "AllowAboutBlank";
    case DecisionSource::kCacheWithUserConfirmation:
      return "CacheWithUserConfirmation";
    case DecisionSource::kCacheWithoutUserConfirmation:
      return "CacheWithoutUserConfirmation";
    case DecisionSource::kEnterprisePolicy:
      return "EnterprisePolicy";
    case DecisionSource::kForbidNonLocalhostIpAddress:
      return "ForbidNonLocalhostIpAddress";
    case DecisionSource::kRequireHttpsOrLocalhost:
      return "RequireHttpsOrLocalhost";
    case DecisionSource::kRequireHttpsOrHttp:
      return "RequireHttpsOrHttp";
    case DecisionSource::kBlockByTaskPolicyConfig:
      return "BlockByTaskPolicyConfig";
    case DecisionSource::kAllowByTaskPolicyConfig:
      return "AllowByTaskPolicyConfig";
    case DecisionSource::kNoVerdict:
      return "NoVerdict";
  }
}

}  // namespace origin_gating
