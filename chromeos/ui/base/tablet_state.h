// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_BASE_TABLET_STATE_H_
#define CHROMEOS_UI_BASE_TABLET_STATE_H_

#include "base/component_export.h"
#include "build/chromeos_buildflags.h"
#include "ui/display/display_observer.h"
#include "ui/display/tablet_state.h"

namespace chromeos {

// This class is DEPRECATED. Do NOT use this in a new place.
// Please use display::Screen::GetScreen()->GetTabletState() to get the state
// and display::Screen::GetScreen()->InTabletMode() to check whether it's in
// tablet mode.
// TODO(elkurin): Remove this class.
//
// Singleton class providing getter methods to access the tablet mode state
// which returns the tablet state stored in display:Screen for lacros-chrome and
// display::DisplayManager for ash-chrome.
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

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;

 private:
  display::ScopedDisplayObserver display_observer_{this};
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_BASE_TABLET_STATE_H_
