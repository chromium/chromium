// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_REQUEST_ACCESS_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_REQUEST_ACCESS_BUTTON_H_

#include "chrome/browser/ui/views/toolbar/toolbar_button.h"

namespace content {
class WebContents;
}  // namespace content

class ToolbarActionViewController;
class Browser;

// Button in the toolbar bar that displays the extensions that requests access,
// and grants them access.
class ExtensionsRequestAccessButton : public ToolbarButton {
 public:
  explicit ExtensionsRequestAccessButton(Browser* browser);
  ExtensionsRequestAccessButton(const ExtensionsRequestAccessButton&) = delete;
  const ExtensionsRequestAccessButton& operator=(
      const ExtensionsRequestAccessButton&) = delete;
  ~ExtensionsRequestAccessButton() override;

  // Updates the extensions requesting access, which also updates the button
  // label.
  void UpdateExtensionsRequestingAccess(
      std::vector<ToolbarActionViewController*> extensions_requesting_access);

  void MaybeShowHoverCard();

  std::vector<std::string> GetExtensionsNamesForTesting();

  // views::View:
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  // ToolbarButton:
  std::u16string GetTooltipText(const gfx::Point& p) const override;

 private:
  void OnButtonPressed();

  content::WebContents* GetActiveWebContents();

  raw_ptr<Browser> browser_;

  std::vector<ToolbarActionViewController*> extensions_requesting_access_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_REQUEST_ACCESS_BUTTON_H_
