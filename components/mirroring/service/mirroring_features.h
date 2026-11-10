// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_MIRRORING_FEATURES_H_
#define COMPONENTS_MIRRORING_SERVICE_MIRRORING_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace mirroring::features {
// TODO(crbug.com/40255351): Should be removed once working properly.
COMPONENT_EXPORT(MIRRORING_SERVICE)
BASE_DECLARE_FEATURE(kCastEnableStreamingWithHiDPI);

COMPONENT_EXPORT(MIRRORING_SERVICE)
BASE_DECLARE_FEATURE(kCastStreaming60fps);

// Killswitch for two stage offer change.
// crbug.com/510476629
COMPONENT_EXPORT(MIRRORING_SERVICE)
BASE_DECLARE_FEATURE(kCastStreamingOfferHardwareFirst);

// Sends NV12 frames to the encoder without converting to I420 first.
// Remove after M170. crbug.com/321259270
COMPONENT_EXPORT(MIRRORING_SERVICE)
BASE_DECLARE_FEATURE(kCastMirroringNativeNV12);

// Killswitch for gradual bandwidth backoff and fast recovery on congestion.
// When disabled, the usable bandwidth snaps down to the estimate whenever
// video frames are dropped. crbug.com/572902679
COMPONENT_EXPORT(MIRRORING_SERVICE)
BASE_DECLARE_FEATURE(kCastStreamingGradualBandwidthBackoff);

// Which video frame drops count as a congestion signal.
enum class CongestionSignal {
  // Frames dropped either before or after encoding.
  kBeforeOrAfterEncoding,
  // Frames dropped before encoding, because too many frames or too much
  // media duration would be in flight. This is a lagging signal.
  kBeforeEncoding,
  // Encoded frames rejected by the Open Screen sender, e.g. because too much
  // media duration is already in flight.
  kAfterEncoding,
};

// The following parameters of kCastStreamingGradualBandwidthBackoff tune the
// bandwidth backoff and recovery. Values outside of a sane range are clamped.
COMPONENT_EXPORT(MIRRORING_SERVICE)
extern const base::FeatureParam<CongestionSignal>
    kCastStreamingCongestionSignal;

// The fraction of the bandwidth estimate that may be used.
COMPONENT_EXPORT(MIRRORING_SERVICE)
extern const base::FeatureParam<double> kCastStreamingGoodNetworkCitizenFactor;

// On congestion, the usable bandwidth is multiplied by a factor between these
// two values, depending on how far below it the estimate is.
COMPONENT_EXPORT(MIRRORING_SERVICE)
extern const base::FeatureParam<double> kCastStreamingMinBackoffFactor;
COMPONENT_EXPORT(MIRRORING_SERVICE)
extern const base::FeatureParam<double> kCastStreamingMaxBackoffFactor;

// The per-update increase while recovering to the bandwidth in use before the
// last backoff.
COMPONENT_EXPORT(MIRRORING_SERVICE)
extern const base::FeatureParam<double> kCastStreamingFastRecoveryIncrease;

// The per-update increase once the previous bandwidth has been recovered.
COMPONENT_EXPORT(MIRRORING_SERVICE)
extern const base::FeatureParam<double> kCastStreamingConservativeIncrease;

}  // namespace mirroring::features

#endif  // COMPONENTS_MIRRORING_SERVICE_MIRRORING_FEATURES_H_
