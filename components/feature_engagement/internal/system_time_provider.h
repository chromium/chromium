// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_SYSTEM_TIME_PROVIDER_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_SYSTEM_TIME_PROVIDER_H_

#include "base/time/time.h"
#include "components/feature_engagement/internal/time_provider.h"

namespace feature_engagement {

// A TimeProvider that uses the system time.
class SystemTimeProvider : public TimeProvider {
 public:
  SystemTimeProvider();

  SystemTimeProvider(const SystemTimeProvider&) = delete;
  SystemTimeProvider& operator=(const SystemTimeProvider&) = delete;

  ~SystemTimeProvider() override;

  // TimeProvider implementation.
  uint32_t GetCurrentDay() const override;
  base::Time Now() const override;
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_SYSTEM_TIME_PROVIDER_H_
