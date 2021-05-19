// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/full_restore/full_restore_info.h"

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "components/account_id/account_id.h"

namespace full_restore {

// static
FullRestoreInfo* FullRestoreInfo::GetInstance() {
  static base::NoDestructor<FullRestoreInfo> full_restore_info;
  return full_restore_info.get();
}

FullRestoreInfo::FullRestoreInfo() = default;

FullRestoreInfo::~FullRestoreInfo() = default;

void FullRestoreInfo::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FullRestoreInfo::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool FullRestoreInfo::ShouldRestore(const AccountId& account_id) {
  return base::Contains(restore_flags_, account_id);
}

void FullRestoreInfo::SetRestoreFlag(const AccountId& account_id,
                                     bool should_restore) {
  if (should_restore == ShouldRestore(account_id))
    return;

  if (should_restore)
    restore_flags_.insert(account_id);
  else
    restore_flags_.erase(account_id);

  for (auto& observer : observers_)
    observer.OnRestoreFlagChanged(account_id, should_restore);
}

bool FullRestoreInfo::CanPerformRestore(const AccountId& account_id) {
  return base::Contains(restore_prefs_, account_id);
}

void FullRestoreInfo::SetRestorePref(const AccountId& account_id,
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

void FullRestoreInfo::OnAppLaunched(aura::Window* window) {
  for (auto& observer : observers_)
    observer.OnAppLaunched(window);
}

void FullRestoreInfo::OnWindowInitialized(aura::Window* window) {
  for (auto& observer : observers_)
    observer.OnWindowInitialized(window);
}

void FullRestoreInfo::OnWidgetInitialized(views::Widget* widget) {
  for (auto& observer : observers_)
    observer.OnWidgetInitialized(widget);
}

void FullRestoreInfo::OnARCTaskReadyForUnparentedWindow(aura::Window* window) {
  for (auto& observer : observers_)
    observer.OnARCTaskReadyForUnparentedWindow(window);
}

}  // namespace full_restore
