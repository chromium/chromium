// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_REQUEST_ACCESS_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_REQUEST_ACCESS_BUTTON_H_

#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "extensions/common/extension_id.h"

namespace content {
class WebContents;
}  // namespace content

class Browser;
class ExtensionsContainer;
class ExtensionsRequestAccessHoverCardCoordinator;

// Button in the toolbar bar that displays the extensions that requests
// access, and are allowed to do so, and grants them access.
class ExtensionsRequestAccessButton : public ToolbarButton {
 public:
  explicit ExtensionsRequestAccessButton(
      Browser* browser,
      ExtensionsContainer* extensions_container);
  ExtensionsRequestAccessButton(const ExtensionsRequestAccessButton&) = delete;
  const ExtensionsRequestAccessButton& operator=(
      const ExtensionsRequestAccessButton&) = delete;
  ~ExtensionsRequestAccessButton() override;

  // Updates the button visibility and content given `extension_ids`.
  void Update(std::vector<extensions::ExtensionId>& extension_ids);

  // Displays the button's hover card, if possible.
  void MaybeShowHoverCard();

  // views::View:
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  // ToolbarButton:
  std::u16string GetTooltipText(const gfx::Point& p) const override;

  // Accessors used by tests:
  std::vector<extensions::ExtensionId> GetExtensionIdsForTesting() {
    return extension_ids_;
  }
  ExtensionsRequestAccessHoverCardCoordinator*
  GetHoverCardCoordinatorForTesting() {
    return hover_card_coordinator_.get();
  }

 private:
  // Runs `extension_ids_` actions in the current site.
  void OnButtonPressed();

  content::WebContents* GetActiveWebContents();

  raw_ptr<Browser> browser_;
  raw_ptr<ExtensionsContainer> extensions_container_;

  std::unique_ptr<ExtensionsRequestAccessHoverCardCoordinator>
      hover_card_coordinator_;

  // Extensions included in the request access button.
  std::vector<extensions::ExtensionId> extension_ids_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_REQUEST_ACCESS_BUTTON_H_
