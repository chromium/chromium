// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_BROWSER_FINGERPRINTING_PROTECTION_FILTER_FEATURES_H_
#define COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_BROWSER_FINGERPRINTING_PROTECTION_FILTER_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace subresource_filter::mojom {
enum class ActivationLevel;
}  // namespace subresource_filter::mojom

namespace fingerprinting_protection_filter::features {

// The primary toggle to enable/disable the Fingerprinting Protection Filter.
COMPONENT_EXPORT(FINGERPRINTING_PROTECTION_FILTER_FEATURES)
BASE_DECLARE_FEATURE(kEnableFingerprintingProtectionFilter);

COMPONENT_EXPORT(FINGERPRINTING_PROTECTION_FILTER_FEATURES)
extern const base::FeatureParam<subresource_filter::mojom::ActivationLevel>
    kActivationLevel;

}  // namespace fingerprinting_protection_filter::features

#endif  // COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_BROWSER_FINGERPRINTING_PROTECTION_FILTER_FEATURES_H_
