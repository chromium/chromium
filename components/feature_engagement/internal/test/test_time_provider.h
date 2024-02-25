// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_TEST_TEST_TIME_PROVIDER_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_TEST_TEST_TIME_PROVIDER_H_

#include "components/feature_engagement/internal/time_provider.h"

namespace feature_engagement {

class TestTimeProvider : public TimeProvider {
 public:
  void SetCurrentDay(uint32_t current_day);

  void SetCurrentTime(base::Time now);

  // TimeProvider
  uint32_t GetCurrentDay() const override;
  base::Time Now() const override;

 private:
  uint32_t current_day_ = 0u;
  base::Time now_ = base::Time::Now();
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_TEST_TEST_TIME_PROVIDER_H_
