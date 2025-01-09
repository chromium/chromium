// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_GLIC_NUDGE_OBSERVER_H_
#define CHROME_BROWSER_UI_TABS_GLIC_NUDGE_OBSERVER_H_

#include "base/observer_list_types.h"

class GlicNudgeObserver : public base::CheckedObserver {
 public:
  // Called when all checks pass to be able to show the glic nudge UI.
  virtual void OnTriggerGlicNudgeUI(std::string label) {}
};

#endif  // CHROME_BROWSER_UI_TABS_GLIC_NUDGE_OBSERVER_H_
