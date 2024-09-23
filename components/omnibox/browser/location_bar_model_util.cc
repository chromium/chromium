// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/location_bar_model_util.h"

#include "base/feature_list.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "components/omnibox/browser/buildflags.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/security_state/core/security_state.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/vector_icon_types.h"

#if (!BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !BUILDFLAG(IS_IOS)
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#include "components/vector_icons/vector_icons.h"     // nogncheck
#endif

namespace location_bar_model {

const gfx::VectorIcon& GetSecurityVectorIcon(
    security_state::SecurityLevel security_level,
    bool use_updated_connection_security_indicators,
    security_state::MaliciousContentStatus malicious_content_status) {
#if (!BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !BUILDFLAG(IS_IOS)
  switch (security_level) {
    case security_state::NONE:
      return omnibox::kHttpChromeRefreshIcon;
    case security_state::SECURE:
      return omnibox::kSecurePageInfoChromeRefreshIcon;
    case security_state::SECURE_WITH_POLICY_INSTALLED_CERT:
      return vector_icons::kBusinessChromeRefreshIcon;
    case security_state::WARNING:
      return vector_icons::kNotSecureWarningChromeRefreshIcon;
    case security_state::DANGEROUS:
      if (malicious_content_status ==
              security_state::MALICIOUS_CONTENT_STATUS_MANAGED_POLICY_WARN ||
          malicious_content_status ==
              security_state::MALICIOUS_CONTENT_STATUS_MANAGED_POLICY_BLOCK) {
        return vector_icons::kBusinessChromeRefreshIcon;
      }
      if (malicious_content_status !=
          security_state::MALICIOUS_CONTENT_STATUS_BILLING) {
        return vector_icons::kDangerousChromeRefreshIcon;
      }
      return vector_icons::kNotSecureWarningChromeRefreshIcon;

    case security_state::SECURITY_LEVEL_COUNT:
      NOTREACHED_IN_MIGRATION();
      return omnibox::kHttpChromeRefreshIcon;
  }
  NOTREACHED_IN_MIGRATION();
  return omnibox::kHttpChromeRefreshIcon;
#else
  NOTREACHED_IN_MIGRATION();
  static const gfx::VectorIcon dummy = {};
  return dummy;
#endif
}

}  // namespace location_bar_model
