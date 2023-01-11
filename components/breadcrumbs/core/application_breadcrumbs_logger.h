// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BREADCRUMBS_CORE_APPLICATION_BREADCRUMBS_LOGGER_H_
#define COMPONENTS_BREADCRUMBS_CORE_APPLICATION_BREADCRUMBS_LOGGER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/metrics/user_metrics.h"

namespace base {
class TimeTicks;
}  // namespace base

namespace breadcrumbs {

class BreadcrumbPersistentStorageManager;

// Listens for and logs application-wide breadcrumb events to the
// BreadcrumbManager.
class ApplicationBreadcrumbsLogger {
 public:
  // Breadcrumbs will be stored in a file in |storage_dir|.
  explicit ApplicationBreadcrumbsLogger(
      const base::FilePath& storage_dir,
      base::RepeatingCallback<bool()> is_metrics_enabled_callback);
  ApplicationBreadcrumbsLogger(const ApplicationBreadcrumbsLogger&) = delete;
  ~ApplicationBreadcrumbsLogger();

  // Returns a pointer to the BreadcrumbPersistentStorageManager owned by this
  // instance. May be null.
  breadcrumbs::BreadcrumbPersistentStorageManager* GetPersistentStorageManager()
      const;

 private:
  // Callback that processes and logs the user action |action| to the
  // BreadcrumbManager.
  void OnUserAction(const std::string& action, base::TimeTicks action_time);

  // Callback which processes and logs memory pressure warnings to the
  // BreadcrumbManager.
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  // Returns true if |action| (UMA User Action) is user triggered.
  static bool IsUserTriggeredAction(const std::string& action);

  // The callback invoked whenever a user action is registered.
  base::ActionCallback user_action_callback_;
  // A memory pressure listener which observes memory pressure events.
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  // A strong pointer to the persistent breadcrumb manager listening for events
  // from the BreadcrumbManager to store to disk.
  std::unique_ptr<breadcrumbs::BreadcrumbPersistentStorageManager>
      persistent_storage_manager_;
};

}  // namespace breadcrumbs

#endif  // COMPONENTS_BREADCRUMBS_CORE_APPLICATION_BREADCRUMBS_LOGGER_H_
