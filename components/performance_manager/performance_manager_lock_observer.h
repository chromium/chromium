// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_LOCK_OBSERVER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_LOCK_OBSERVER_H_

#include "content/public/browser/lock_observer.h"

namespace performance_manager {

class PerformanceManagerLockObserver : public content::LockObserver {
 public:
  PerformanceManagerLockObserver();
  ~PerformanceManagerLockObserver() override;

  // content::LockObserver:
  void OnFrameStartsHoldingWebLocks(int render_process_id,
                                    int render_frame_id) override;
  void OnFrameStopsHoldingWebLocks(int render_process_id,
                                   int render_frame_id) override;
  void OnFrameStartsHoldingIndexedDBConnections(int render_process_id,
                                                int render_frame_id) override;
  void OnFrameStopsHoldingIndexedDBConnections(int render_process_id,
                                               int render_frame_id) override;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_LOCK_OBSERVER_H_
