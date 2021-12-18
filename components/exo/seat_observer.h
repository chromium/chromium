// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SEAT_OBSERVER_H_
#define COMPONENTS_EXO_SEAT_OBSERVER_H_

namespace exo {

class Surface;

// Observers can listen to various events on the Seats.
class SeatObserver {
 public:
  // Called when a new surface receives keyboard focus.
  virtual void OnSurfaceFocused(Surface* gained_focus,
                                Surface* lost_focus,
                                bool has_focused_client) = 0;

 protected:
  virtual ~SeatObserver() = default;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SEAT_OBSERVER_H_
