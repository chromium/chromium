// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_ICON_STATE_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_ICON_STATE_HELPER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/security_state/core/security_state.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
namespace ui {
class ImageModel;
}

class LocationBarModel;
class AutocompleteClassifier;
class OmniboxController;

namespace content {
class WebContents;
}

namespace location_bar {

struct SecurityChipAccessibilityState {
  ax::mojom::Role role;
  std::u16string name;
  std::u16string description;
};

// Text Content & Visibility
std::u16string GetSecurityChipText(const LocationBarModel* model,
                                   content::WebContents* web_contents,
                                   bool is_editing_or_empty);

// Returns whether the location bar should display the security chip
// text for the page state described by `model`. Covers special schemes
// (chrome://, chrome-extension://, file://, dom-distiller://),
// contextual-tasks pages, security warnings/errors, and pages rendered
// by a generic MIME handler extension. `is_editing_or_empty` suppresses
// the chip while the user is interacting with the omnibox.
// `web_contents` may be nullptr; when non-null it enables checks that
// require live tab state (currently the MIME handler extension path).
bool ShouldShowSecurityChipText(const LocationBarModel* model,
                                content::WebContents* web_contents,
                                bool is_editing_or_empty);

// Returns the resource ID if the icon is the Google Super G gradient icon;
// nullopt otherwise.
std::optional<int> MaybeGetGradientGoogleSuperGIcon(const ui::ImageModel& icon);

// Accessibility & Tooltip
SecurityChipAccessibilityState GetSecurityChipAccessibilityState(
    const LocationBarModel* model,
    bool is_editing_or_empty,
    std::u16string_view current_label);

std::u16string GetSecurityChipTooltipText(bool is_editing_or_empty);

// Animation Rules
bool ShouldAnimateSecurityChipTextChange(
    bool is_editing_or_empty,
    security_state::SecurityLevel previous_level,
    security_state::SecurityLevel new_level);

// Execute a Paste-and-Go action on the omnibox from a middle click.
void ExecutePasteAndGo(OmniboxController& omnibox_controller,
                       AutocompleteClassifier* autocomplete_classifier,
                       const std::u16string& text,
                       base::TimeTicks event_timestamp);

// Evaluates if the OS supports pasting from the selection clipboard on
// middle-click, and if so, asynchronously reads the clipboard and routes the
// text to `paste_callback`. Returns true if the event was intercepted and the
// clipboard read was dispatched.
bool InitiateMiddleClickPasteIfSupported(
    bool is_middle_click,
    base::OnceCallback<void(std::u16string)> paste_callback);

}  // namespace location_bar

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_ICON_STATE_HELPER_H_
