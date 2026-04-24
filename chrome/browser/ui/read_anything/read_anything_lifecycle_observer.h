// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_LIFECYCLE_OBSERVER_H_
#define CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_LIFECYCLE_OBSERVER_H_

#include "base/observer_list_types.h"
#include "chrome/browser/ui/read_anything/read_anything_enums.h"

#include <optional>

// Class of events tracking the lifecycle of the Reading Mode application.
// Events that track the browser/tab (e.g. TabWillDetach) do not belong in this
// class.
class ReadAnythingLifecycleObserver : public base::CheckedObserver {
 public:
  virtual void Activate(bool active,
                        std::optional<ReadAnythingOpenTrigger> trigger) {}
  virtual void OnDestroyed() = 0;
  virtual void OnReadingModePresenterChanged() {}
  virtual void OnWillClose(ReadAnythingCloseReason reason) {}
};

#endif  // CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_LIFECYCLE_OBSERVER_H_
