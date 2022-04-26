// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_request_access_button.h"
#include <memory>

#include "base/bind.h"
#include "base/check_op.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/extensions/extensions_request_access_button_hover_card.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

ExtensionsRequestAccessButton::ExtensionsRequestAccessButton(Browser* browser)
    : ToolbarButton(
          base::BindRepeating(&ExtensionsRequestAccessButton::OnButtonPressed,
                              base::Unretained(this))),
      browser_(browser) {}

ExtensionsRequestAccessButton::~ExtensionsRequestAccessButton() = default;

void ExtensionsRequestAccessButton::UpdateExtensionsRequestingAccess(
    std::vector<ToolbarActionViewController*> extensions_requesting_access) {
  DCHECK(!extensions_requesting_access.empty());
  extensions_requesting_access_ = extensions_requesting_access;

  // TODO(crbug.com/1239772): Set the label and background color without borders
  // separately to match the mocks. For now, using SetHighlight to display that
  // adds a border and highlight color in addition to the label.
  absl::optional<SkColor> color;
  SetHighlight(l10n_util::GetStringFUTF16Int(
                   IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON,
                   static_cast<int>(extensions_requesting_access_.size())),
               color);
}

void ExtensionsRequestAccessButton::ShowHoverCard() {
  content::WebContents* web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  ExtensionsRequestAccessButtonHoverCard::ShowBubble(
      web_contents, this, extensions_requesting_access_);
}

std::u16string ExtensionsRequestAccessButton::GetTooltipText(
    const gfx::Point& p) const {
  // Request access button hover cards replace tooltips.
  return std::u16string();
}

// TODO(crbug.com/1239772): Grant access to all the extensions requesting access
// when the button is pressed.
void ExtensionsRequestAccessButton::OnButtonPressed() {}
