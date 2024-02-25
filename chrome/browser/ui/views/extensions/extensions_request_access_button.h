// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_REQUEST_ACCESS_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_REQUEST_ACCESS_BUTTON_H_

#include <optional>

#include "base/timer/timer.h"
#include "chrome/browser/ui/views/toolbar/toolbar_chip_button.h"
#include "extensions/common/extension_id.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "url/origin.h"

namespace content {
class WebContents;
}  // namespace content

class Browser;
class ExtensionsContainer;
class ExtensionsRequestAccessHoverCardCoordinator;

// Button in the toolbar bar that displays the extensions that requests
// access, and are allowed to do so, and grants them access.
class ExtensionsRequestAccessButton : public ToolbarChipButton {
  METADATA_HEADER(ExtensionsRequestAccessButton, ToolbarChipButton)

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

  // Hides the button and resets the `confirmation_origin_` variable.
  void ResetConfirmation();

  // Returns whether the button is showing a confirmation message.
  bool IsShowingConfirmation() const;

  // Returns whether the button is showing a confirmation message for `origin`.
  bool IsShowingConfirmationFor(const url::Origin& origin) const;

  // Returns the number of extensions included in the button.
  size_t GetExtensionsCount() const;

  // ToolbarButton:
  std::u16string GetTooltipText(const gfx::Point& p) const override;
  bool ShouldShowInkdropAfterIphInteraction() override;

  // Accessors used by tests:
  std::vector<extensions::ExtensionId> GetExtensionIdsForTesting() {
    return extension_ids_;
  }
  ExtensionsRequestAccessHoverCardCoordinator*
  GetHoverCardCoordinatorForTesting() {
    return hover_card_coordinator_.get();
  }
  void remove_confirmation_for_testing(bool remove_confirmation) {
    remove_confirmation_for_testing_ = remove_confirmation;
  }

 private:
  // Grants one-time site access to `extension_ids` and shows a confirmation
  // message on the button.
  void OnButtonPressed();

  content::WebContents* GetActiveWebContents() const;

  raw_ptr<Browser> browser_;
  raw_ptr<ExtensionsContainer> extensions_container_;

  std::unique_ptr<ExtensionsRequestAccessHoverCardCoordinator>
      hover_card_coordinator_;

  // Extensions included in the request access button.
  std::vector<extensions::ExtensionId> extension_ids_;

  // The origin for which the button is displaying a confirmation message, if
  // any.
  std::optional<url::Origin> confirmation_origin_;

  // A timer used to collapse the button after showing a confirmation message.
  base::OneShotTimer collapse_timer_;

  // Flag to not show confirmation message in tests.
  bool remove_confirmation_for_testing_{false};
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_REQUEST_ACCESS_BUTTON_H_
