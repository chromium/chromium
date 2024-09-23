// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_TESTING_CLOCK_TIME_PROVIDER_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_TESTING_CLOCK_TIME_PROVIDER_H_

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/feature_engagement/internal/time_provider.h"

namespace base {
class Clock;
}

namespace feature_engagement {

// A TimeProvider that uses a testing clock time.
class TestingClockTimeProvider : public TimeProvider {
 public:
  // The passed in clock must outlive this class.
  TestingClockTimeProvider(const base::Clock& clock LIFETIME_BOUND,
                           base::Time initial_now);

  TestingClockTimeProvider(const TestingClockTimeProvider&) = delete;
  TestingClockTimeProvider& operator=(const TestingClockTimeProvider&) = delete;

  ~TestingClockTimeProvider() override;

  // TimeProvider implementation.
  uint32_t GetCurrentDay() const override;
  base::Time Now() const override;

 private:
  const base::Clock& clock_;
  base::Time initial_now_;
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_TESTING_CLOCK_TIME_PROVIDER_H_
