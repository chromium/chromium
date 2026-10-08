// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/network_header_injection/core/features.h"

#include "build/build_config.h"

namespace enterprise_custom_headers {

BASE_FEATURE(kHttpHeadersInjection,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

bool IsHttpHeaderInjectionEnabled() {
  return base::FeatureList::IsEnabled(kHttpHeadersInjection);
}

}  // namespace enterprise_custom_headers
