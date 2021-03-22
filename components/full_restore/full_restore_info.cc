// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/full_restore/full_restore_info.h"

#include "base/no_destructor.h"
#include "components/account_id/account_id.h"

namespace full_restore {

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
  return restore_flags_.find(account_id) != restore_flags_.end();
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

}  // namespace full_restore
