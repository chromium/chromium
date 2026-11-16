// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/early_metrics_guard.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/types/optional_util.h"
#include "components/metrics/environment_recorder.h"

namespace metrics {
namespace {

EarlyMetricsGuard* g_active_guard = nullptr;

}  // namespace

EarlyMetricsGuard::EarlyMetricsGuard(PrefService* local_state)
    : local_state_(local_state) {
  CHECK(!g_active_guard);
  g_active_guard = this;
}

EarlyMetricsGuard::~EarlyMetricsGuard() {
  CHECK_EQ(g_active_guard, this);
  g_active_guard = nullptr;
}

// static
bool EarlyMetricsGuard::IsEarlyMetricsRecordingActive() {
  return g_active_guard != nullptr;
}

// static
const SystemProfileProto* EarlyMetricsGuard::GetCachedSystemProfile() {
  if (!g_active_guard) {
    return nullptr;
  }
  return g_active_guard->GetOrLoadCachedSystemProfile();
}

const SystemProfileProto* EarlyMetricsGuard::GetOrLoadCachedSystemProfile() {
  if (!local_state_) {
    return nullptr;
  }
  if (!cache_loaded_) {
    cache_loaded_ = true;
    SystemProfileProto profile;
    EnvironmentRecorder recorder(local_state_);
    if (recorder.LoadEnvironmentFromPrefs(&profile)) {
      cached_system_profile_ = std::move(profile);
    }
  }
  return base::OptionalToPtr(cached_system_profile_);
}

}  // namespace metrics
