// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/base/tablet_state.h"

#include "base/check_op.h"
#include "ui/display/screen.h"

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
  display::Screen::GetScreen()->AddObserver(this);
}

TabletState::~TabletState() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
  display::Screen::GetScreen()->RemoveObserver(this);
}

bool TabletState::InTabletMode() const {
  return state_ == display::TabletState::kInTabletMode ||
         state_ == display::TabletState::kEnteringTabletMode;
}

void TabletState::OnDisplayTabletStateChanged(display::TabletState state) {
  state_ = state;
}

}  // namespace chromeos
