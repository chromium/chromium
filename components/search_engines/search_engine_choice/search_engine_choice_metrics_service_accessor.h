// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_METRICS_SERVICE_ACCESSOR_H_
#define COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_METRICS_SERVICE_ACCESSOR_H_

#include "base/gtest_prod_util.h"
#include "components/metrics/metrics_service_accessor.h"

namespace search_engines {

// This class limits and documents access to metrics service helper methods.
// Since these methods are private, each user has to be explicitly declared
// as a 'friend' below.
class SearchEngineChoiceMetricsServiceAccessor
    : public metrics::MetricsServiceAccessor {
 private:
  friend class SearchEngineChoiceService;
  friend class SearchEngineChoiceServiceTest;
  FRIEND_TEST_ALL_PREFIXES(
      SearchEngineChoiceServiceTest,
      MaybeRecordChoiceScreenDisplayState_OnServiceStartup_UmaDisabled);
};

}  // namespace search_engines

#endif  // COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_METRICS_SERVICE_ACCESSOR_H_
