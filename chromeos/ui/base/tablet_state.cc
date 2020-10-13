// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/base/tablet_state.h"

#include "base/check_op.h"

namespace chromeos {

namespace {
TabletState* g_instance = nullptr;
}

TabletState* TabletState::Get() {
  return g_instance;
}

TabletState::TabletState() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

TabletState::~TabletState() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

void TabletState::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TabletState::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool TabletState::InTabletMode() const {
  return state_ == TabletState::kInTabletMode ||
         state_ == TabletState::kEnteringTabletMode;
}

void TabletState::SetState(State state) {
  state_ = state;

  for (auto& observer : observers_)
    observer.OnTabletStateChanged(state_);
}

}  // namespace chromeos
