// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_BASE_TABLET_STATE_H_
#define CHROMEOS_UI_BASE_TABLET_STATE_H_

#include "base/component_export.h"
#include "ui/display/display_observer.h"
#include "ui/display/tablet_state.h"

namespace chromeos {

// Class is a singleton and holds the tablet mode state.
//
// TODO(crbug.com/1113900): Move the logic to display::Screen::GetTabletState()
// and implement it for Ash and Ozone child classes.
class COMPONENT_EXPORT(CHROMEOS_UI_BASE) TabletState
    : public display::DisplayObserver {
 public:
  // Returns the singleton instance.
  static TabletState* Get();

  TabletState();
  TabletState(const TabletState&) = delete;
  TabletState& operator=(const TabletState&) = delete;
  ~TabletState() override;

  // Returns true if the system is in tablet mode.
  bool InTabletMode() const;

  display::TabletState state() const { return state_; }

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;

 private:
  display::ScopedDisplayObserver display_observer_{this};

  display::TabletState state_ = display::TabletState::kInClamshellMode;
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_BASE_TABLET_STATE_H_
