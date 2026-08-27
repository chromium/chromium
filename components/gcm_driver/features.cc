// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace gcm {

namespace features {

namespace {

constexpr int kDefaultGCMMessageBufferingTTLSeconds = 45;

constexpr base::FeatureParam<int> kGCMMessageBufferingTTLSeconds{
    &kGCMMessageBuffering, "message_buffering_ttl_seconds",
    kDefaultGCMMessageBufferingTTLSeconds};

}  // namespace

BASE_FEATURE(kInvalidateTokenFeature,
             "GCMTokenInvalidAfterDays",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGCMMessageBuffering, base::FEATURE_DISABLED_BY_DEFAULT);

const char kParamNameTokenInvalidationPeriodDays[] =
    "token_invalidation_period";
// A token invalidation period of 0 means the feature is disabled, and the
// GCM token never becomes stale.
const int kDefaultTokenInvalidationPeriod = 7;

base::TimeDelta GetTokenInvalidationInterval() {
  if (!base::FeatureList::IsEnabled(kInvalidateTokenFeature)) {
    return base::TimeDelta();
  }
  int override_value_days = base::GetFieldTrialParamByFeatureAsInt(
      kInvalidateTokenFeature, kParamNameTokenInvalidationPeriodDays,
      kDefaultTokenInvalidationPeriod);
  return base::Days(override_value_days);
}

base::TimeDelta GetGCMMessageBufferingTTL() {
  int ttl_seconds = kGCMMessageBufferingTTLSeconds.Get();
  if (ttl_seconds <= 0) {
    ttl_seconds = kDefaultGCMMessageBufferingTTLSeconds;
  }
  return base::Seconds(ttl_seconds);
}

}  // namespace features

}  // namespace gcm
