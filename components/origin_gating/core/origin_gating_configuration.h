// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_GATING_CORE_ORIGIN_GATING_CONFIGURATION_H_
#define COMPONENTS_ORIGIN_GATING_CORE_ORIGIN_GATING_CONFIGURATION_H_

#include <initializer_list>
#include <vector>

#include "components/origin_gating/core/types.h"

namespace origin_gating {

class OriginGatingConfiguration {
 public:
  // `predicates` specifies the ordered sequence of decision predicates to
  // execute.
  //
  // The following internal/fallback states are strictly forbidden:
  // - `DecisionSource::kNoVerdict`
  OriginGatingConfiguration(std::initializer_list<DecisionSource> predicates,
                            bool use_site_keyed_cache);
  ~OriginGatingConfiguration();

  OriginGatingConfiguration(const OriginGatingConfiguration&);
  OriginGatingConfiguration& operator=(const OriginGatingConfiguration&);

  const std::vector<DecisionSource>& predicates() const { return predicates_; }
  bool use_site_keyed_cache() const { return use_site_keyed_cache_; }

 private:
  std::vector<DecisionSource> predicates_;
  bool use_site_keyed_cache_ = false;
};

}  // namespace origin_gating

#endif  // COMPONENTS_ORIGIN_GATING_CORE_ORIGIN_GATING_CONFIGURATION_H_
