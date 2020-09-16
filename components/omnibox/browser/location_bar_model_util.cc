// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/location_bar_model_util.h"

#include "build/build_config.h"
#include "components/omnibox/browser/buildflags.h"
#include "ui/gfx/vector_icon_types.h"

#if (!defined(OS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !defined(OS_IOS)
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#include "components/vector_icons/vector_icons.h"     // nogncheck
#endif

namespace location_bar_model {

const gfx::VectorIcon& GetSecurityVectorIcon(
    security_state::SecurityLevel security_level) {
#if (!defined(OS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !defined(OS_IOS)
  switch (security_level) {
    case security_state::NONE:
      return omnibox::kHttpIcon;
    case security_state::WARNING:
      // When kMarkHttpAsParameterDangerWarning is enabled, show a danger
      // triangle icon.
      if (security_state::ShouldShowDangerTriangleForWarningLevel()) {
        return omnibox::kNotSecureWarningIcon;
      }
      NOTREACHED();
      return omnibox::kHttpIcon;
    case security_state::SECURE:
      return omnibox::kHttpsValidIcon;
    case security_state::SECURE_WITH_POLICY_INSTALLED_CERT:
      return vector_icons::kBusinessIcon;
    case security_state::DANGEROUS:
      return omnibox::kNotSecureWarningIcon;
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
