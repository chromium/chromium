// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_GLIC_NUDGE_OBSERVER_H_
#define CHROME_BROWSER_UI_TABS_GLIC_NUDGE_OBSERVER_H_

#include "base/observer_list_types.h"

class GlicNudgeObserver : public base::CheckedObserver {
 public:
  // Called when the glic nudge UI needs to be triggered, or to be turned off.
  // When the UI needs to be shown `label' holds the nudge label. When the nudge
  // UI should be turned off, `label` is empty.
  virtual void OnTriggerGlicNudgeUI(std::string label) {}
};

#endif  // CHROME_BROWSER_UI_TABS_GLIC_NUDGE_OBSERVER_H_
