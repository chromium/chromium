// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_BASE_TABLET_STATE_H_
#define CHROMEOS_UI_BASE_TABLET_STATE_H_

#include "base/component_export.h"
#include "base/observer_list.h"

namespace ash {
class TabletModeController;
}

namespace chromeos {

// Class is a singleton and holds the tablet mode state.
//
// The idea is that only the creator of this class in Ash or Lacros/Ozone code
// is able to set the state.
class COMPONENT_EXPORT(CHROMEOS_UI_BASE) TabletState {
 public:
  // Returns the singleton instance.
  static TabletState* Get();

  // Tracks whether we are in the process of entering or exiting tablet mode.
  // Used for logging histogram metrics.
  enum State {
    kInClamshellMode,
    kEnteringTabletMode,
    kInTabletMode,
    kExitingTabletMode,
  };

  class COMPONENT_EXPORT(CHROMEOS_UI_BASE) Observer {
   public:
    virtual void OnTabletStateChanged(State state) = 0;

   protected:
    virtual ~Observer() = default;
  };

  TabletState();
  TabletState(const TabletState&) = delete;
  TabletState& operator=(const TabletState&) = delete;
  ~TabletState();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns true if the system is in tablet mode.
  bool InTabletMode() const;

  State state() const { return state_; }

 private:
  // The friend class declaration here is used to control classes that can set
  // the tablet state.
  friend class ash::TabletModeController;

  void SetState(State state);

  base::ObserverList<Observer>::Unchecked observers_;

  State state_ = State::kInClamshellMode;
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_BASE_TABLET_STATE_H_
