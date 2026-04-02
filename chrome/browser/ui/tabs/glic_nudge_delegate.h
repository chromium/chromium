// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_GLIC_NUDGE_DELEGATE_H_
#define CHROME_BROWSER_UI_TABS_GLIC_NUDGE_DELEGATE_H_

#include <optional>
#include <string>

class GlicNudgeDelegate {
 public:
  virtual ~GlicNudgeDelegate() = 0;
  // Called when the glic nudge UI needs to be triggered. `label' holds the
  // nudge label.
  virtual void OnTriggerGlicNudgeUI(std::string label) = 0;
  // Show an anchored message bubble via the page action framework.
  // `label` is the clickable chip text. `anchored_message_text` is the bubble
  // description. `prompt_suggestion` is the prompt to pass to ToggleUI when the
  // cue is clicked, captured at trigger time to avoid races with controller
  // state changes (e.g., navigation, tab switch).
  virtual void OnTriggerAnchoredMessage(
      std::string label,
      std::string anchored_message_text,
      std::optional<std::string> prompt_suggestion) = 0;
  // Called when the glic nudge UI needs to be hidden.
  virtual void OnHideGlicNudgeUI() = 0;
  // Called when we want to check if the UI is currently showing.
  virtual bool GetIsShowingGlicNudge() = 0;
};

#endif  // CHROME_BROWSER_UI_TABS_GLIC_NUDGE_DELEGATE_H_
