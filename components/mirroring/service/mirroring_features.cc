// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/mirroring_features.h"

#include <array>

namespace mirroring::features {
// This flag enables HiDPI capture during Cast Streaming mirroring sessions.
//
// This feature is enabled by the Chrome command line flag
// --enable-cast-streaming-with-hidpi.
BASE_FEATURE(kCastEnableStreamingWithHiDPI, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables 60FPS support for Cast Mirroring sessions.
BASE_FEATURE(kCastStreaming60fps, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCastStreamingOfferHardwareFirst,
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, NV12 video frames will be sent to the encoder without being
// converted to I420.
BASE_FEATURE(kCastMirroringNativeNV12, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCastStreamingGradualBandwidthBackoff,
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr auto kCongestionSignalOptions =
    std::to_array<base::FeatureParam<CongestionSignal>::Option>({
        {CongestionSignal::kBeforeOrAfterEncoding, "before_or_after_encoding"},
        {CongestionSignal::kBeforeEncoding, "before_encoding"},
        {CongestionSignal::kAfterEncoding, "after_encoding"},
    });
const base::FeatureParam<CongestionSignal> kCastStreamingCongestionSignal{
    &kCastStreamingGradualBandwidthBackoff, "congestion_signal",
    CongestionSignal::kBeforeOrAfterEncoding, kCongestionSignalOptions};

const base::FeatureParam<double> kCastStreamingGoodNetworkCitizenFactor{
    &kCastStreamingGradualBandwidthBackoff, "good_network_citizen_factor", 0.8};
const base::FeatureParam<double> kCastStreamingMinBackoffFactor{
    &kCastStreamingGradualBandwidthBackoff, "min_backoff_factor", 0.5};
const base::FeatureParam<double> kCastStreamingMaxBackoffFactor{
    &kCastStreamingGradualBandwidthBackoff, "max_backoff_factor", 0.85};
const base::FeatureParam<double> kCastStreamingFastRecoveryIncrease{
    &kCastStreamingGradualBandwidthBackoff, "fast_recovery_increase", 1.5};
const base::FeatureParam<double> kCastStreamingConservativeIncrease{
    &kCastStreamingGradualBandwidthBackoff, "conservative_increase", 1.1};

}  // namespace mirroring::features
