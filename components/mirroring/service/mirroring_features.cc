// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/mirroring_features.h"
#include "third_party/libaom/libaom_buildflags.h"

namespace mirroring {
namespace features {

// Controls whether offers using the AV1 codec for video encoding are included
// in mirroring negotiations in addition to the VP8 codec, or offers only
// include VP8.
const base::Feature kCastStreamingAv1{"CastStreamingAv1",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether offers using the VP9 codec for video encoding are included
// in mirroring negotiations in addition to the VP8 codec, or offers only
// include VP8.
const base::Feature kCastStreamingVp9{"CastStreamingVp9",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the allow list (legacy behavior) or blocklist is used to
// determine whether remoting capabilities should be queried for as part of
// configuring a mirroring session.
const base::Feature kCastUseBlocklistForRemotingQuery{
    "CastUseBlocklistForRemotingQuery", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables querying for remoting capabilities against ALL devices, as opposed to
// just those controlled by the allow or blocklist. When set, this flag takes
// precedence over the above flag.
const base::Feature kCastForceEnableRemotingQuery{
    "CastForceEnableRemotingQuery", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsCastStreamingAV1Enabled() {
#if BUILDFLAG(ENABLE_LIBAOM)
  return base::FeatureList::IsEnabled(features::kCastStreamingAv1);
#else
  return false;
#endif
}

}  // namespace features
}  // namespace mirroring
