// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_GLIC_NUDGE_DELEGATE_H_
#define CHROME_BROWSER_UI_TABS_GLIC_NUDGE_DELEGATE_H_

#include <string>

class GlicNudgeDelegate {
 public:
  virtual ~GlicNudgeDelegate() = 0;
  // Called when the glic nudge UI needs to be triggered. `label' holds the
  // nudge label.
  virtual void OnTriggerGlicNudgeUI(std::string label) = 0;
  // Called when the glic nudge UI needs to be hidden.
  virtual void OnHideGlicNudgeUI() = 0;
  // Called when we want to check if the UI is currently showing.
  virtual bool GetIsShowingGlicNudge() = 0;
};

#endif  // CHROME_BROWSER_UI_TABS_GLIC_NUDGE_DELEGATE_H_
