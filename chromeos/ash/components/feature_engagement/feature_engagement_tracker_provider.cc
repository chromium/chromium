// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/feature_engagement/feature_engagement_tracker_provider.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"

namespace ash {
namespace {
FeatureEngagementTrackerProvider* g_instance = nullptr;
}  // namespace

FeatureEngagementTrackerProvider::FeatureEngagementTrackerProvider() {
  CHECK(!g_instance);
  g_instance = this;
}

FeatureEngagementTrackerProvider::~FeatureEngagementTrackerProvider() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
FeatureEngagementTrackerProvider& FeatureEngagementTrackerProvider::Get() {
  return CHECK_DEREF(g_instance);
}

}  // namespace ash
