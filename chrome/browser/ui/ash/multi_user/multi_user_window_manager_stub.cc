// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_stub.h"

#include "base/logging.h"
#include "components/account_id/account_id.h"

MultiUserWindowManagerStub::MultiUserWindowManagerStub() {}

MultiUserWindowManagerStub::~MultiUserWindowManagerStub() {}

void MultiUserWindowManagerStub::SetWindowOwner(aura::Window* window,
                                                const AccountId& account_id) {
  NOTIMPLEMENTED();
}

const AccountId& MultiUserWindowManagerStub::GetWindowOwner(
    const aura::Window* window) const {
  return EmptyAccountId();
}

void MultiUserWindowManagerStub::ShowWindowForUser(
    aura::Window* window,
    const AccountId& account_id) {
  NOTIMPLEMENTED();
}

bool MultiUserWindowManagerStub::AreWindowsSharedAmongUsers() const {
  return false;
}

std::set<AccountId> MultiUserWindowManagerStub::GetOwnersOfVisibleWindows()
    const {
  return {};
}

const AccountId& MultiUserWindowManagerStub::GetUserPresentingWindow(
    const aura::Window* window) const {
  return EmptyAccountId();
}

void MultiUserWindowManagerStub::AddObserver(
    ash::MultiUserWindowManagerObserver* observer) {
  NOTIMPLEMENTED();
}

void MultiUserWindowManagerStub::RemoveObserver(
    ash::MultiUserWindowManagerObserver* observer) {
  NOTIMPLEMENTED();
}

const AccountId& MultiUserWindowManagerStub::CurrentAccountId() const {
  return EmptyAccountId();
}
