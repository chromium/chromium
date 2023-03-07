// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_request_access_button.h"

#include <algorithm>
#include <iterator>
#include <memory>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_request_access_hover_card_coordinator.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

std::vector<const extensions::Extension*> GetExtensions(
    Profile* profile,
    std::vector<extensions::ExtensionId>& extension_ids) {
  const extensions::ExtensionSet& enabled_extensions =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions();
  std::vector<const extensions::Extension*> extensions;
  for (auto extension_id : extension_ids) {
    extensions.push_back(enabled_extensions.GetByID(extension_id));
  }
  return extensions;
}

}  // namespace

ExtensionsRequestAccessButton::ExtensionsRequestAccessButton(
    Browser* browser,
    ExtensionsContainer* extensions_container)
    : ToolbarButton(
          base::BindRepeating(&ExtensionsRequestAccessButton::OnButtonPressed,
                              base::Unretained(this))),
      browser_(browser),
      extensions_container_(extensions_container),
      hover_card_coordinator_(
          std::make_unique<ExtensionsRequestAccessHoverCardCoordinator>()) {}

ExtensionsRequestAccessButton::~ExtensionsRequestAccessButton() = default;

void ExtensionsRequestAccessButton::Update(
    std::vector<extensions::ExtensionId>& extension_ids) {
  extension_ids_ = extension_ids;
  SetVisible(!extension_ids_.empty());

  if (extension_ids_.empty()) {
    return;
  }

  // TODO(crbug.com/1239772): Set the label and background color without borders
  // separately to match the mocks. For now, using SetHighlight to display that
  // adds a border and highlight color in addition to the label.
  absl::optional<SkColor> color;
  SetHighlight(
      l10n_util::GetStringFUTF16Int(IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON,
                                    static_cast<int>(extension_ids_.size())),
      color);
}

void ExtensionsRequestAccessButton::MaybeShowHoverCard() {
  if (hover_card_coordinator_->IsShowing() ||
      !GetWidget()->IsMouseEventsEnabled()) {
    return;
  }

  hover_card_coordinator_->ShowBubble(GetActiveWebContents(), this,
                                      extensions_container_, extension_ids_);
}

std::u16string ExtensionsRequestAccessButton::GetTooltipText(
    const gfx::Point& p) const {
  // Request access button hover cards replace tooltips.
  return std::u16string();
}

void ExtensionsRequestAccessButton::OnButtonPressed() {
  if (hover_card_coordinator_->IsShowing()) {
    hover_card_coordinator_->HideBubble();
  }

  content::WebContents* web_contents = GetActiveWebContents();
  extensions::ExtensionActionRunner* action_runner =
      extensions::ExtensionActionRunner::GetForWebContents(web_contents);
  if (!action_runner) {
    return;
  }

  DCHECK_GT(extension_ids_.size(), 0u);
  std::vector<const extensions::Extension*> extensions_to_run =
      GetExtensions(browser_->profile(), extension_ids_);

  base::RecordAction(base::UserMetricsAction(
      "Extensions.Toolbar.ExtensionsActivatedFromRequestAccessButton"));
  action_runner->GrantTabPermissions(extensions_to_run);
}

// Linux enter/leave events are sometimes flaky, so we don't want to "miss"
// an enter event and fail to hover the button. This is effectively a no-op if
// the button is already showing the hover card (crbug.com/1326272).
void ExtensionsRequestAccessButton::OnMouseMoved(const ui::MouseEvent& event) {
  MaybeShowHoverCard();
}

void ExtensionsRequestAccessButton::OnMouseEntered(
    const ui::MouseEvent& event) {
  MaybeShowHoverCard();
}

void ExtensionsRequestAccessButton::OnMouseExited(const ui::MouseEvent& event) {
  hover_card_coordinator_->HideBubble();
}

content::WebContents* ExtensionsRequestAccessButton::GetActiveWebContents() {
  return browser_->tab_strip_model()->GetActiveWebContents();
}
