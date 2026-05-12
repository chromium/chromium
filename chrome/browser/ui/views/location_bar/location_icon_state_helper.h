// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_ICON_STATE_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_ICON_STATE_HELPER_H_

#include <string>

#include "components/security_state/core/security_state.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
namespace ui {
class ImageModel;
}

class LocationBarModel;

namespace content {
class WebContents;
}

namespace location_bar {

enum class SecurityChipIcon {
  kHttp,
  kSecurePageInfo,
  kNotSecureWarning,
  kDangerous,
  kAddContext,
};

struct SecurityChipAccessibilityState {
  ax::mojom::Role role;
  std::u16string name;
  std::u16string description;
};

// Text Content & Visibility
std::u16string GetSecurityChipText(const LocationBarModel* model,
                                   content::WebContents* web_contents,
                                   bool is_editing_or_empty);

bool ShouldShowSecurityChipText(const LocationBarModel* model,
                                bool is_editing_or_empty);

// Semantic Icon & Interactivity
// GetSecurityChipIconEnum returns the corresponding semantic icon shape.
// Native Views uses the enum to drive its legacy SkColor logic, WebUI
// passes the enum directly to its frontend via Mojo.
SecurityChipIcon GetSecurityChipIconEnum(const LocationBarModel* model,
                                         bool is_add_context_button_shown);

// Returns true if the icon is the Google Super G gradient icon.
bool IsGradientGoogleSuperGIcon(const ui::ImageModel& icon);

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

}  // namespace location_bar

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_ICON_STATE_HELPER_H_
