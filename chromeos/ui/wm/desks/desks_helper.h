// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_WM_DESKS_DESKS_HELPER_H_
#define CHROMEOS_UI_WM_DESKS_DESKS_HELPER_H_

#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "build/chromeos_buildflags.h"

namespace aura {
class Window;
}  // namespace aura

namespace chromeos {

// Interface for an chromeos client (e.g. Chrome) to interact with the Virtual
// Desks feature.
class COMPONENT_EXPORT(CHROMEOS_UI_WM) DesksHelper {
 public:
  // Returns DesksHelper instance by window.
  // It is OK to pass nullptr when called from ash if there is no accessible
  // window at that moment.
  // But window must be passed to get DesksHelper when called from lacros.
  static DesksHelper* Get(aura::Window* window);

  DesksHelper(const DesksHelper&) = delete;
  DesksHelper& operator=(const DesksHelper&) = delete;

  // Returns true if |window| exists on the currently active desk.
  virtual bool BelongsToActiveDesk(aura::Window* window) = 0;

  // Returns the active desk's index.
  virtual int GetActiveDeskIndex() const = 0;

  // Returns the names of the desk at |index|. If |index| is out-of-bounds,
  // return empty string.
  virtual std::u16string GetDeskName(int index) const = 0;

  // Returns the number of desks.
  virtual int GetNumberOfDesks() const = 0;

  // Sends |window| to desk at |desk_index|. Does nothing if the desk at
  // |desk_index| is the active desk. |desk_index| must be valid.
  virtual void SendToDeskAtIndex(aura::Window* window, int desk_index) = 0;

 protected:
  DesksHelper();
  virtual ~DesksHelper();
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_WM_DESKS_DESKS_HELPER_H_
