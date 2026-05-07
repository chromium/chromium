// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_LIFECYCLE_OBSERVER_H_
#define CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_LIFECYCLE_OBSERVER_H_

#include <optional>

#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "chrome/browser/ui/read_anything/read_anything_enums.h"

// Class of events tracking the lifecycle of the Reading Mode application.
// Events that track the browser/tab (e.g. TabWillDetach) do not belong in this
// class.
class ReadAnythingLifecycleObserver : public base::CheckedObserver {
 public:
  // Invoked when the active state of the Reading Mode application changes.
  // `active`: True if the UI is becoming active/shown, false if it is hidden.
  // `trigger`: The entry point trigger that initiated the activation (only
  //            populated when `active` is true).
  // `completed_session_duration`: The total duration that Reading Mode was
  //                              visible during the completed session (only
  //                              populated when `active` is false, and not
  //                              during in-progress presentation mode
  //                              transitions or if closed before being
  //                              successfully shown).
  virtual void Activate(
      bool active,
      std::optional<ReadAnythingOpenTrigger> trigger,
      std::optional<base::TimeDelta> completed_session_duration) {}
  virtual void OnDestroyed() = 0;
  virtual void OnReadingModePresenterChanged() {}
  virtual void OnWillClose(ReadAnythingCloseReason reason) {}
};

#endif  // CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_LIFECYCLE_OBSERVER_H_
