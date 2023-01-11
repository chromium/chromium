// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEAVY_AD_INTERVENTION_HEAVY_AD_SERVICE_H_
#define COMPONENTS_HEAVY_AD_INTERVENTION_HEAVY_AD_SERVICE_H_

#include <memory>

#include "base/functional/callback.h"
#include "components/blocklist/opt_out_blocklist/opt_out_blocklist_delegate.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
class FilePath;
}

namespace heavy_ad_intervention {

class HeavyAdBlocklist;

// Keyed service that owns the heavy ad intervention blocklist.
class HeavyAdService : public KeyedService,
                       public blocklist::OptOutBlocklistDelegate {
 public:
  HeavyAdService();

  HeavyAdService(const HeavyAdService&) = delete;
  HeavyAdService& operator=(const HeavyAdService&) = delete;

  ~HeavyAdService() override;

  // Initializes the UI Service. |profile_path| is the path to user data on
  // disk.
  void Initialize(const base::FilePath& profile_path);

  // Initializes the blocklist with no backing store for incognito mode.
  void InitializeOffTheRecord();

  // |on_blocklist_loaded_callback| will be invoked when the heavy ad blocklist
  // has been loaded. It will be invoked immediately if the blocklist has
  // already been loaded and has not subsequently been unloaded.
  void NotifyOnBlocklistLoaded(base::OnceClosure on_blocklist_loaded_callback);

  // |on_blocklist_cleared| will be invoked when the heavy ad blocklist
  // is cleared.
  void NotifyOnBlocklistCleared(
      base::OnceClosure on_blocklist_cleared_callback);

  HeavyAdBlocklist* heavy_ad_blocklist() { return heavy_ad_blocklist_.get(); }

 private:
  // blocklist::OptOutBlocklistDelegate:
  void OnLoadingStateChanged(bool is_loaded) override;
  void OnBlocklistCleared(base::Time time) override;

  // The blocklist used to control triggering of the heavy ad intervention.
  // Created during Initialize().
  std::unique_ptr<HeavyAdBlocklist> heavy_ad_blocklist_;

  base::OnceClosure on_blocklist_loaded_callback_;
  base::OnceClosure on_blocklist_cleared_callback_;
  bool blocklist_is_loaded_ = false;
};

}  // namespace heavy_ad_intervention

#endif  // COMPONENTS_HEAVY_AD_INTERVENTION_HEAVY_AD_SERVICE_H_
