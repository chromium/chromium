// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_FEATURES_H_
#define COMPONENTS_PLUS_ADDRESSES_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace plus_addresses {

COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
BASE_DECLARE_FEATURE(kFeature);

// Used to control the enterprise plus address feature's autofill suggestion
// label. Defaults to generic Lorem Ipsum as strings are not yet determined.
COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
extern const base::FeatureParam<std::string>
    kEnterprisePlusAddressLabelOverride;

// Used to control the enterprise plus address feature's OAuth scope.
COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
extern const base::FeatureParam<std::string> kEnterprisePlusAddressOAuthScope;

// The url that the enterprise uses to create plus addresses. Must be a valid
// GURL, such as `https://foo.example/`.
COMPONENT_EXPORT(PLUS_ADDRESSES_FEATURES)
extern const base::FeatureParam<std::string> kEnterprisePlusAddressServerUrl;
}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_FEATURES_H_
