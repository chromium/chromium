// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_restore/app_restore_info.h"

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "components/account_id/account_id.h"

namespace app_restore {

// static
AppRestoreInfo* AppRestoreInfo::GetInstance() {
  static base::NoDestructor<AppRestoreInfo> app_restore_info;
  return app_restore_info.get();
}

AppRestoreInfo::AppRestoreInfo() = default;

AppRestoreInfo::~AppRestoreInfo() = default;

void AppRestoreInfo::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AppRestoreInfo::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool AppRestoreInfo::CanPerformRestore(const AccountId& account_id) {
  return base::Contains(restore_prefs_, account_id);
}

void AppRestoreInfo::SetRestorePref(const AccountId& account_id,
                                    bool could_restore) {
  if (could_restore == CanPerformRestore(account_id))
    return;

  if (could_restore)
    restore_prefs_.insert(account_id);
  else
    restore_prefs_.erase(account_id);

  for (auto& observer : observers_)
    observer.OnRestorePrefChanged(account_id, could_restore);
}

void AppRestoreInfo::OnAppLaunched(aura::Window* window) {
  for (auto& observer : observers_)
    observer.OnAppLaunched(window);
}

void AppRestoreInfo::OnWindowInitialized(aura::Window* window) {
  for (auto& observer : observers_)
    observer.OnWindowInitialized(window);
}

void AppRestoreInfo::OnWidgetInitialized(views::Widget* widget) {
  for (auto& observer : observers_)
    observer.OnWidgetInitialized(widget);
}

void AppRestoreInfo::OnParentWindowToValidContainer(aura::Window* window) {
  for (auto& observer : observers_)
    observer.OnParentWindowToValidContainer(window);
}

}  // namespace app_restore
