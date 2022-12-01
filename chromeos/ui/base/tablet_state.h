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

  display::TabletState state() const;

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Enables/disables tablet mode on client side. Not thet this does not modify
  // server side tablet state.
  //
  // DO NOT use this for integration tests such as browser tests. Use this only
  // on unit-testing.
  // Use TestController crosapi EnterTabletMode/ExitTabletMode if Ash server is
  // available since the test may depend on server side behavior.
  void EnableTabletModeForTesting(bool enable);
#endif

 private:
  display::ScopedDisplayObserver display_observer_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_BASE_TABLET_STATE_H_
