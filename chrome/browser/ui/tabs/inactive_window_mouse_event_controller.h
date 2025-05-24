// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_INACTIVE_WINDOW_MOUSE_EVENT_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_INACTIVE_WINDOW_MOUSE_EVENT_CONTROLLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"

namespace tabs {

class InactiveWindowMouseEventController;

// A scoped helper class that manages the lifetime of mouse event acceptance
// for a InactiveWindowMouseEventController object. It increments the counter
// upon construction and decrements it upon destruction.
class ScopedAcceptMouseEventsWhileWindowInactive {
 public:
  explicit ScopedAcceptMouseEventsWhileWindowInactive(
      base::WeakPtr<InactiveWindowMouseEventController> controller);
  ~ScopedAcceptMouseEventsWhileWindowInactive();

 private:
  base::WeakPtr<InactiveWindowMouseEventController> controller_;
};

// Manages the state related to accepting mouse events
// while the window is inactive. It utilizes a counter mechanism that is
// controlled by the ScopedAcceptMouseEventsWhileWindowInactive helper.
class InactiveWindowMouseEventController {
 public:
  InactiveWindowMouseEventController();
  ~InactiveWindowMouseEventController();

  bool ShouldAcceptMouseEventsWhileWindowInactive() const;
  std::unique_ptr<ScopedAcceptMouseEventsWhileWindowInactive>
  AcceptMouseEventsWhileWindowInactive();

  base::WeakPtr<InactiveWindowMouseEventController> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Only ScopedAcceptMouseEventsWhileWindowInactive can Increment and
  // Decrement.
  class PassKey {
   public:
    PassKey(const PassKey& other) = delete;
    PassKey& operator=(const PassKey& other) = delete;

   private:
    friend class ScopedAcceptMouseEventsWhileWindowInactive;
    PassKey() = default;
  };

  // Updates the count of ScopedAcceptMouseEventsWhileWindowInactive.
  void Increment(PassKey key);
  void Decrement(PassKey key);

 private:
  // Count of ScopedAcceptMouseEventsWhileWindowInactives that are constructed.
  // When this value is 0 the application wont accept mouse events while the
  // window is inactive. When the counter is greater than 0 it will on Mac. see
  // chrome/browser/renderer_host/chrome_render_widget_host_view_mac_delegate.mm
  int accept_input_counter_ = 0;

  base::WeakPtrFactory<InactiveWindowMouseEventController> weak_ptr_factory_{
      this};
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_INACTIVE_WINDOW_MOUSE_EVENT_CONTROLLER_H_
