// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lookalikes/core/features.h"

namespace lookalikes {
namespace features {

// Note: this flag is ignored on iOS. See lookalike_url_util.cc.
const base::Feature kDetectTargetEmbeddingLookalikes{
    "TargetEmbeddingLookalikes", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kLookalikeInterstitialForPunycode{
    "LookalikeInterstitialForPunycode", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kLookalikeDigitalAssetLinks{
    "LookalikeDigitalAssetLinks", base::FEATURE_DISABLED_BY_DEFAULT};

const char kLookalikeDigitalAssetLinksTimeoutParameter[] = "timeout";

}  // namespace features
}  // namespace lookalikes
