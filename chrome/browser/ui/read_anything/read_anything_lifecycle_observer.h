// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_LIFECYCLE_OBSERVER_H_
#define CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_LIFECYCLE_OBSERVER_H_

#include "base/observer_list_types.h"
#include "chrome/browser/ui/read_anything/read_anything_enums.h"

#include <optional>

class ReadAnythingLifecycleObserver : public base::CheckedObserver {
 public:
  virtual void Activate(bool active,
                        std::optional<ReadAnythingOpenTrigger> trigger) {}
  virtual void OnDestroyed() = 0;
  virtual void OnTabWillDetach() {}
  virtual void OnReadingModePresenterChanged() {}
};

#endif  // CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_LIFECYCLE_OBSERVER_H_
