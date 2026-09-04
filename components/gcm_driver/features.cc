// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/features.h"

#include "base/feature_list.h"
#include "base/time/time.h"

namespace gcm::features {

namespace {

constexpr int kDefaultGCMMessageBufferingTTLSeconds = 45;

constexpr base::FeatureParam<int> kGCMMessageBufferingTTLSeconds{
    &kGCMMessageBuffering, "message_buffering_ttl_seconds",
    kDefaultGCMMessageBufferingTTLSeconds};

}  // namespace

BASE_FEATURE(kGCMMessageBuffering, base::FEATURE_DISABLED_BY_DEFAULT);

base::TimeDelta GetGCMMessageBufferingTTL() {
  int ttl_seconds = kGCMMessageBufferingTTLSeconds.Get();
  if (ttl_seconds <= 0) {
    ttl_seconds = kDefaultGCMMessageBufferingTTLSeconds;
  }
  return base::Seconds(ttl_seconds);
}

}  // namespace gcm::features
