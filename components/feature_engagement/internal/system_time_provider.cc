// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/system_time_provider.h"

#include "base/numerics/clamped_math.h"
#include "base/time/time.h"

namespace feature_engagement {

SystemTimeProvider::SystemTimeProvider() = default;

SystemTimeProvider::~SystemTimeProvider() = default;

uint32_t SystemTimeProvider::GetCurrentDay() const {
  base::TimeDelta delta = Now() - base::Time::UnixEpoch();
  return base::saturated_cast<uint32_t>(delta.InDays());
}

base::Time SystemTimeProvider::Now() const {
  return base::Time::Now();
}

}  // namespace feature_engagement
