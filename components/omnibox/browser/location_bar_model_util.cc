// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/location_bar_model_util.h"

#include "base/feature_list.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "components/omnibox/browser/buildflags.h"
#include "components/omnibox/common/omnibox_features.h"
#include "ui/gfx/vector_icon_types.h"

#if (!BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !BUILDFLAG(IS_IOS)
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#include "components/vector_icons/vector_icons.h"     // nogncheck
#endif

namespace location_bar_model {

const gfx::VectorIcon& GetSecurityVectorIcon(
    security_state::SecurityLevel security_level,
    bool use_updated_connection_security_indicators) {
#if (!BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !BUILDFLAG(IS_IOS)
  switch (security_level) {
    case security_state::NONE:
      return omnibox::kHttpIcon;
    case security_state::SECURE: {
      return use_updated_connection_security_indicators
                 ? vector_icons::kHttpsValidArrowIcon
                 : vector_icons::kHttpsValidIcon;
    }
    case security_state::SECURE_WITH_POLICY_INSTALLED_CERT:
      return vector_icons::kBusinessIcon;
    case security_state::WARNING:
    case security_state::DANGEROUS:
      return vector_icons::kNotSecureWarningIcon;
    case security_state::SECURITY_LEVEL_COUNT:
      NOTREACHED();
      return omnibox::kHttpIcon;
  }
  NOTREACHED();
  return omnibox::kHttpIcon;
#else
  NOTREACHED();
  static const gfx::VectorIcon dummy = {};
  return dummy;
#endif
}

}  // namespace location_bar_model
