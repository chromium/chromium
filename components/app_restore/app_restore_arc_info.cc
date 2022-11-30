// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_restore/app_restore_arc_info.h"

#include "base/no_destructor.h"

namespace app_restore {

// static
AppRestoreArcInfo* AppRestoreArcInfo::GetInstance() {
  static base::NoDestructor<AppRestoreArcInfo> app_restore_arc_info;
  return app_restore_arc_info.get();
}

AppRestoreArcInfo::AppRestoreArcInfo() = default;

AppRestoreArcInfo::~AppRestoreArcInfo() = default;

void AppRestoreArcInfo::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AppRestoreArcInfo::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AppRestoreArcInfo::NotifyTaskCreated(const std::string& app_id,
                                          int32_t task_id,
                                          int32_t session_id) {
  for (auto& observer : observers_)
    observer.OnTaskCreated(app_id, task_id, session_id);
}

void AppRestoreArcInfo::NotifyTaskDestroyed(int32_t task_id) {
  for (auto& observer : observers_)
    observer.OnTaskDestroyed(task_id);
}

void AppRestoreArcInfo::NotifyArcConnectionChanged(bool is_connection_ready) {
  for (auto& observer : observers_)
    observer.OnArcConnectionChanged(is_connection_ready);
}

void AppRestoreArcInfo::NotifyPlayStoreEnabledChanged(bool enabled) {
  for (auto& observer : observers_)
    observer.OnArcPlayStoreEnabledChanged(enabled);
}

void AppRestoreArcInfo::NotifyTaskThemeColorUpdated(int32_t task_id,
                                                    uint32_t primary_color,
                                                    uint32_t status_bar_color) {
  for (auto& observer : observers_)
    observer.OnTaskThemeColorUpdated(task_id, primary_color, status_bar_color);
}

}  // namespace app_restore
