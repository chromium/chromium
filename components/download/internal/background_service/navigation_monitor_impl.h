// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_NAVIGATION_MONITOR_IMPL_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_NAVIGATION_MONITOR_IMPL_H_

#include "base/cancelable_callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/download/public/background_service/navigation_monitor.h"

namespace download {

class NavigationMonitorImpl : public NavigationMonitor {
 public:
  NavigationMonitorImpl();

  NavigationMonitorImpl(const NavigationMonitorImpl&) = delete;
  NavigationMonitorImpl& operator=(const NavigationMonitorImpl&) = delete;

  ~NavigationMonitorImpl() override;

  // NavigationMonitor implementation.
  void SetObserver(NavigationMonitor::Observer* observer) override;
  void OnNavigationEvent(NavigationEvent event) override;
  bool IsNavigationInProgress() const override;
  void Configure(base::TimeDelta navigation_completion_delay,
                 base::TimeDelta navigation_timeout_delay) override;

 private:
  void NotifyNavigationFinished();
  void ScheduleBackupTask();
  void OnNavigationFinished();

  raw_ptr<NavigationMonitor::Observer> observer_;

  int current_navigation_count_;

  base::CancelableOnceClosure navigation_finished_callback_;

  base::CancelableOnceClosure backup_navigation_finished_callback_;

  base::TimeDelta navigation_completion_delay_;
  base::TimeDelta navigation_timeout_delay_;

  base::WeakPtrFactory<NavigationMonitorImpl> weak_ptr_factory_{this};
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_NAVIGATION_MONITOR_IMPL_H_
