// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/network_header_injection/core/features.h"

namespace enterprise_custom_headers {

BASE_FEATURE(kHttpHeadersInjection, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsHttpHeaderInjectionEnabled() {
  return base::FeatureList::IsEnabled(kHttpHeadersInjection);
}

}  // namespace enterprise_custom_headers
