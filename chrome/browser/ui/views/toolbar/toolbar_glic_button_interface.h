// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_GLIC_BUTTON_INTERFACE_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_GLIC_BUTTON_INTERFACE_H_

#include <string>

namespace glic {

// Abstract interface for controlling the toolbar Glic button, implemented by
// both the Views (`ToolbarGlicButton`) and WebUI (`WebUIGlicControl`) controls.
class ToolbarGlicButtonInterface {
 public:
  virtual void SetIsShowingNudge(bool is_showing) = 0;
  virtual bool GetIsShowingNudge() const = 0;
  virtual void SetNudgeLabel(std::string label) = 0;
  virtual void SetVisible(bool visible) = 0;
  virtual void SetGlicPanelIsOpen(bool open) = 0;
  virtual void UpdateStyle(bool should_match_toolbar) = 0;

 protected:
  ~ToolbarGlicButtonInterface() = default;
};

}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_GLIC_BUTTON_INTERFACE_H_
