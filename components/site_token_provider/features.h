// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SITE_TOKEN_PROVIDER_FEATURES_H_
#define COMPONENTS_SITE_TOKEN_PROVIDER_FEATURES_H_

#include "base/feature_list.h"

namespace site_token_provider::features {

BASE_DECLARE_FEATURE(kSiteTokenProviderEnabled);

extern const base::FeatureParam<std::string> kSiteTokenOAuth2Scope;

extern const base::FeatureParam<std::string> kSiteTokenEndpointUrl;

}  // namespace site_token_provider::features

#endif  // COMPONENTS_SITE_TOKEN_PROVIDER_FEATURES_H_
