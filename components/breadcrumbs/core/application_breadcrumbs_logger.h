// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BREADCRUMBS_CORE_APPLICATION_BREADCRUMBS_LOGGER_H_
#define COMPONENTS_BREADCRUMBS_CORE_APPLICATION_BREADCRUMBS_LOGGER_H_

#include <memory>
#include <string>

#include "base/memory/memory_pressure_listener.h"
#include "base/metrics/user_metrics.h"

namespace base {
class TimeTicks;
}  // namespace base

namespace breadcrumbs {

class BreadcrumbManager;
class BreadcrumbPersistentStorageManager;

// Listens for and logs application wide breadcrumb events to the
// BreadcrumbManager passed in the constructor.
class ApplicationBreadcrumbsLogger {
 public:
  explicit ApplicationBreadcrumbsLogger(
      breadcrumbs::BreadcrumbManager* breadcrumb_manager);
  ApplicationBreadcrumbsLogger(const ApplicationBreadcrumbsLogger&) = delete;
  ~ApplicationBreadcrumbsLogger();

  // Sets a BreadcrumbPersistentStorageManager to persist application breadcrumb
  // events logged by this ApplicationBreadcrumbsLogger instance.
  void SetPersistentStorageManager(
      std::unique_ptr<breadcrumbs::BreadcrumbPersistentStorageManager>
          persistent_storage_manager);

  // Returns a pointer to the BreadcrumbPersistentStorageManager owned by this
  // instance. May be null.
  breadcrumbs::BreadcrumbPersistentStorageManager* GetPersistentStorageManager()
      const;

 protected:
  // Adds an event to |breadcrumb_manager_|.
  void AddEvent(const std::string& event);

 private:
  // Callback which processes and logs the user action |action| to
  // |breadcrumb_manager_|.
  void OnUserAction(const std::string& action, base::TimeTicks action_time);

  // Callback which processes and logs memory pressure warnings to
  // |breadcrumb_manager_|.
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  // Returns true if |action| (UMA User Action) is user triggered.
  static bool IsUserTriggeredAction(const std::string& action);

  // The BreadcrumbManager to log events.
  breadcrumbs::BreadcrumbManager* breadcrumb_manager_;
  // The callback invoked whenever a user action is registered.
  base::ActionCallback user_action_callback_;
  // A memory pressure listener which observes memory pressure events.
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  // A strong pointer to the persistent breadcrumb manager listening for events
  // from |breadcrumb_manager_| to store to disk.
  std::unique_ptr<breadcrumbs::BreadcrumbPersistentStorageManager>
      persistent_storage_manager_;
};

}  // namespace breadcrumbs

#endif  // COMPONENTS_BREADCRUMBS_CORE_APPLICATION_BREADCRUMBS_LOGGER_H_
