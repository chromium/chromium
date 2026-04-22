// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RECORD_REPLAY_CORE_BROWSER_ACTIVITY_HARDCODED_PROVIDER_H_
#define COMPONENTS_RECORD_REPLAY_CORE_BROWSER_ACTIVITY_HARDCODED_PROVIDER_H_

#include "components/record_replay/core/browser/activity_provider.h"

namespace record_replay {

class ActivityHardcodedProvider : public ActivityProvider {
 public:
  ActivityHardcodedProvider();
  ~ActivityHardcodedProvider() override;

  // ActivityProvider:
  void ShouldOfferActivity(
      const GURL& url,
      base::OnceCallback<
          void(std::optional<ActivityDiscoveryService::AutomationMetadata>)>
          callback) override;
};

}  // namespace record_replay

#endif  // COMPONENTS_RECORD_REPLAY_CORE_BROWSER_ACTIVITY_HARDCODED_PROVIDER_H_
