// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEAVY_AD_INTERVENTION_HEAVY_AD_BLOCKLIST_H_
#define COMPONENTS_HEAVY_AD_INTERVENTION_HEAVY_AD_BLOCKLIST_H_

#include <stdint.h>

#include "base/time/time.h"
#include "components/blocklist/opt_out_blocklist/opt_out_blocklist.h"

namespace base {
class Clock;
}

namespace blocklist {
class OptOutBlocklistDelegate;
class OptOutStore;
}  // namespace blocklist

namespace heavy_ad_intervention {

// The heavy ad intervention only supports one type for the blocklist.
enum class HeavyAdBlocklistType {
  kHeavyAdOnlyType = 0,
};

// A class that manages opt out blocklist parameters for the heavy ad
// intervention. The blocklist is used to allow at most 5 interventions per top
// frame origin per day. This prevents the intervention from being used as a
// cross-origin side channel.
class HeavyAdBlocklist : public blocklist::OptOutBlocklist {
 public:
  HeavyAdBlocklist(std::unique_ptr<blocklist::OptOutStore> opt_out_store,
                   base::Clock* clock,
                   blocklist::OptOutBlocklistDelegate* blocklist_delegate);

  HeavyAdBlocklist(const HeavyAdBlocklist&) = delete;
  HeavyAdBlocklist& operator=(const HeavyAdBlocklist&) = delete;

  ~HeavyAdBlocklist() override;

 protected:
  // OptOutBlocklist:
  bool ShouldUseSessionPolicy(base::TimeDelta* duration,
                              size_t* history,
                              int* threshold) const override;
  bool ShouldUsePersistentPolicy(base::TimeDelta* duration,
                                 size_t* history,
                                 int* threshold) const override;
  bool ShouldUseHostPolicy(base::TimeDelta* duration,
                           size_t* history,
                           int* threshold,
                           size_t* max_hosts) const override;
  bool ShouldUseTypePolicy(base::TimeDelta* duration,
                           size_t* history,
                           int* threshold) const override;
  blocklist::BlocklistData::AllowedTypesAndVersions GetAllowedTypes()
      const override;
};

}  // namespace heavy_ad_intervention

#endif  // COMPONENTS_HEAVY_AD_INTERVENTION_HEAVY_AD_BLOCKLIST_H_
