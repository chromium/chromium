// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LOOKALIKES_CORE_FEATURES_H_
#define COMPONENTS_LOOKALIKES_CORE_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace lookalikes {
namespace features {

// This feature enables interstitial warnings for target embedding lookalikes.
COMPONENT_EXPORT(LOOKALIKES_FEATURES)
extern const base::Feature kDetectTargetEmbeddingLookalikes;

// This feature enables interstitial warnings for certain punycode domains.
COMPONENT_EXPORT(LOOKALIKES_FEATURES)
extern const base::Feature kLookalikeInterstitialForPunycode;

// This feature enables Digital Asset Link validations for lookalikes.
COMPONENT_EXPORT(LOOKALIKES_FEATURES)
extern const base::Feature kLookalikeDigitalAssetLinks;

// Timeout before giving up on Digital Asset Link manifest fetches. The feature
// fetches manifests from both the lookalike and the target URLs. If it fails to
// fetch either manifest within this period, the validation is assumed to fail.
COMPONENT_EXPORT(LOOKALIKES_FEATURES)
extern const char kLookalikeDigitalAssetLinksTimeoutParameter[];

}  // namespace features
}  // namespace lookalikes

#endif  // COMPONENTS_LOOKALIKES_CORE_FEATURES_H_
