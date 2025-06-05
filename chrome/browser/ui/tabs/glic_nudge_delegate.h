// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_GLIC_NUDGE_DELEGATE_H_
#define CHROME_BROWSER_UI_TABS_GLIC_NUDGE_DELEGATE_H_

#include <string>

class GlicNudgeDelegate {
 public:
  virtual ~GlicNudgeDelegate() = 0;
  // Called when the glic nudge UI needs to be triggered, or to be turned off.
  // When the UI needs to be shown `label' holds the nudge label. When the nudge
  // UI should be turned off, `label` is empty.
  virtual void OnTriggerGlicNudgeUI(std::string label) = 0;
  // Called when we want to check if the UI is currently showing.
  virtual bool GetIsShowingGlicNudge() = 0;
};

#endif  // CHROME_BROWSER_UI_TABS_GLIC_NUDGE_DELEGATE_H_
