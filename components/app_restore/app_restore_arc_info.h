// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_RESTORE_APP_RESTORE_ARC_INFO_H_
#define COMPONENTS_APP_RESTORE_APP_RESTORE_ARC_INFO_H_

#include "base/component_export.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace app_restore {

// AppRestoreArcInfo is responsible for providing information about ARC apps to
// its observers.
class COMPONENT_EXPORT(APP_RESTORE) AppRestoreArcInfo {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Invoked when the task is created for an ARC app.
    virtual void OnTaskCreated(const std::string& app_id,
                               int32_t task_id,
                               int32_t session_id) {}

    // Invoked when the task is destroyed for an ARC app.
    virtual void OnTaskDestroyed(int32_t task_id) {}

    // Invoked when the ARC instance connection is ready or closed.
    virtual void OnArcConnectionChanged(bool is_connection_ready) {}

    // Invoked when the task theme colors are updated for an ARC app.
    virtual void OnTaskThemeColorUpdated(int32_t task_id,
                                         uint32_t primary_color,
                                         uint32_t status_bar_color) {}

    // Invoked when Google Play Store is enabled or disabled.
    virtual void OnArcPlayStoreEnabledChanged(bool enabled) {}

   protected:
    ~Observer() override = default;
  };

  static AppRestoreArcInfo* GetInstance();

  AppRestoreArcInfo();
  AppRestoreArcInfo(const AppRestoreArcInfo&) = delete;
  AppRestoreArcInfo& operator=(const AppRestoreArcInfo&) = delete;
  ~AppRestoreArcInfo();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void NotifyTaskCreated(const std::string& app_id,
                         int32_t task_id,
                         int32_t session_id);
  void NotifyTaskDestroyed(int32_t task_id);
  void NotifyArcConnectionChanged(bool is_connection_ready);
  void NotifyPlayStoreEnabledChanged(bool enabled);
  void NotifyTaskThemeColorUpdated(int32_t task_id,
                                   uint32_t primary_color,
                                   uint32_t status_bar_color);

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace app_restore

#endif  // COMPONENTS_APP_RESTORE_APP_RESTORE_ARC_INFO_H_
