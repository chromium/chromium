// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TEST_SUPPORT_GLIC_BUTTON_TEST_ACCESSOR_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TEST_SUPPORT_GLIC_BUTTON_TEST_ACCESSOR_H_

#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"

class BrowserWindowInterface;

namespace content {
class WebContents;
}  // namespace content

namespace glic {
class ToolbarGlicButtonInterface;
}  // namespace glic

namespace views {
class LabelButton;
}  // namespace views

// Test accessor for interacting with and inspecting the Glic toolbar button,
// regardless of whether it is backed by Views or WebUI (`WebUIGlicControl`).
class GlicButtonTestAccessor {
 public:
  explicit GlicButtonTestAccessor(BrowserWindowInterface* browser);
  GlicButtonTestAccessor(const GlicButtonTestAccessor&) = default;
  GlicButtonTestAccessor& operator=(const GlicButtonTestAccessor&) = default;
  ~GlicButtonTestAccessor();

  // Returns the underlying `ToolbarGlicButtonInterface` control.
  glic::ToolbarGlicButtonInterface* GetControl() const;

  // Returns true if the Glic button is currently visible.
  bool IsVisible() const;

  // Waits until the Glic button becomes visible (or hidden).
  [[nodiscard]] bool WaitForVisible() const;
  [[nodiscard]] bool WaitForHidden() const;

  // Returns the current accessible name (`aria-label` in WebUI, cached
  // accessible name in Views) or tooltip text.
  std::string GetAriaLabel() const;
  std::string GetTooltip() const;

  // Waits until the accessible name matches `expected`.
  [[nodiscard]] bool WaitForAriaLabel(std::string_view expected) const;

  // Returns true if the button is in its menu-open / active state
  // (`is-menu-open` attribute in WebUI).
  bool IsMenuOpen() const;

  // Returns true if `aria-expanded` is `"true"` on the WebUI button.
  bool IsAriaExpanded() const;

  // Returns true if the Glic button is collapsed to icon-only mode (no label
  // displayed).
  bool IsCollapsed() const;

  // Waits until the button reaches the collapsed (if `collapsed` is true) or
  // expanded-with-label (if `collapsed` is false) state.
  [[nodiscard]] bool WaitForCollapsed(bool collapsed) const;

  // Simulates a left click or right click (contextmenu) on the Glic button.
  void Click() const;
  void RightClick() const;

 private:
  views::LabelButton* GetViewsButton() const;
  bool HasCollapsedState(bool collapsed) const;
  content::WebContents* GetWebContents() const;

  raw_ptr<BrowserWindowInterface> browser_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TEST_SUPPORT_GLIC_BUTTON_TEST_ACCESSOR_H_
