// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_FEATURE_OBSERVER_CLIENT_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_FEATURE_OBSERVER_CLIENT_H_

#include "content/public/browser/feature_observer_client.h"

namespace performance_manager {

class PerformanceManagerFeatureObserverClient
    : public content::FeatureObserverClient {
 public:
  PerformanceManagerFeatureObserverClient();
  ~PerformanceManagerFeatureObserverClient() override;

  // content::FeatureObserverClient implementation:
  void OnStartUsing(content::GlobalRenderFrameHostId id,
                    blink::mojom::ObservedFeatureType feature_type) override;
  void OnStopUsing(content::GlobalRenderFrameHostId id,
                   blink::mojom::ObservedFeatureType feature_type) override;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_FEATURE_OBSERVER_CLIENT_H_
