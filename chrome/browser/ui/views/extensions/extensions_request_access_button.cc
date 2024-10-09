// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_request_access_button.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_util.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/extensions/extensions_dialogs_utils.h"
#include "chrome/browser/ui/views/extensions/extensions_request_access_hover_card_coordinator.h"
#include "chrome/browser/ui/views/toolbar/toolbar_chip_button.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"

namespace {

// TODO(crbug.com/40916158): Same as permission's ChipController. Pull out to a
// shared location.
constexpr auto kConfirmationDisplayDuration = base::Seconds(4);

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
    : ToolbarChipButton(
          base::BindRepeating(&ExtensionsRequestAccessButton::OnButtonPressed,
                              base::Unretained(this)),
          ToolbarChipButton::Edge::kRight),
      browser_(browser),
      extensions_container_(extensions_container),
      hover_card_coordinator_(
          std::make_unique<ExtensionsRequestAccessHoverCardCoordinator>()) {
  // Set button for IPH.
  SetProperty(views::kElementIdentifierKey,
              kExtensionsRequestAccessButtonElementId);
}

ExtensionsRequestAccessButton::~ExtensionsRequestAccessButton() = default;

void ExtensionsRequestAccessButton::Update(
    std::vector<extensions::ExtensionId>& extension_ids) {
  CHECK(!IsShowingConfirmation());

  extension_ids_ = extension_ids;
  SetVisible(!extension_ids_.empty());

  if (extension_ids_.empty()) {
    return;
  }

  // TODO(crbug.com/40784980): Set the label and background color without
  // borders separately to match the mocks. For now, using SetHighlight to
  // display that adds a border and highlight color in addition to the label.
  std::optional<SkColor> color;
  SetHighlight(
      l10n_util::GetStringFUTF16Int(IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON,
                                    static_cast<int>(extension_ids_.size())),
      color);
  SetEnabled(true);
}

// TODO(crbug.com/40879945): Remove hover card once
// kExtensionsMenuAccessControlWithPermittedSites is rolled out. We are keeping
// it for now since we may bring the hover card back.
void ExtensionsRequestAccessButton::MaybeShowHoverCard() {
  if (hover_card_coordinator_->IsShowing() ||
      !GetWidget()->IsMouseEventsEnabled()) {
    return;
  }

  hover_card_coordinator_->ShowBubble(GetActiveWebContents(), this,
                                      extensions_container_, extension_ids_);
}

void ExtensionsRequestAccessButton::ResetConfirmation() {
  SetVisible(false);
  confirmation_origin_ = std::nullopt;
  collapse_timer_.AbandonAndStop();
}

bool ExtensionsRequestAccessButton::IsShowingConfirmation() const {
  if (!confirmation_origin_.has_value()) {
    return false;
  }

  CHECK(GetVisible());
  return confirmation_origin_.has_value();
}

size_t ExtensionsRequestAccessButton::GetExtensionsCount() const {
  return extension_ids_.size();
}

bool ExtensionsRequestAccessButton::IsShowingConfirmationFor(
    const url::Origin& origin) const {
  if (!confirmation_origin_.has_value()) {
    return false;
  }

  CHECK(GetVisible());
  return confirmation_origin_ == origin;
}

std::u16string ExtensionsRequestAccessButton::GetTooltipText(
    const gfx::Point& p) const {
  std::vector<std::u16string> tooltip_parts;
  content::WebContents* active_contents = GetActiveWebContents();

  // Active contents can be null if the window is closing.
  if (!active_contents) {
    return std::u16string();
  }
  tooltip_parts.push_back(l10n_util::GetStringFUTF16(
      IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON_TOOLTIP_MULTIPLE_EXTENSIONS,
      GetCurrentHost(active_contents)));
  for (const auto& extension_id : extension_ids_) {
    ToolbarActionViewController* action =
        extensions_container_->GetActionForId(extension_id);
    tooltip_parts.push_back(action->GetActionName());
  }
  return base::JoinString(tooltip_parts, u"\n");
}

bool ExtensionsRequestAccessButton::ShouldShowInkdropAfterIphInteraction() {
  return false;
}

void ExtensionsRequestAccessButton::OnButtonPressed() {
  // Record IPH usage.
  browser_->window()->NotifyFeaturePromoFeatureUsed(
      feature_engagement::kIPHExtensionsRequestAccessButtonFeature,
      FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  content::WebContents* web_contents = GetActiveWebContents();
  extensions::ExtensionActionRunner* action_runner =
      extensions::ExtensionActionRunner::GetForWebContents(web_contents);
  if (!action_runner) {
    return;
  }

  // Make sure we set this before granting tab permissions, since that will
  // trigger an update to the request access button for each extension that is
  // granted access.
  confirmation_origin_ =
      web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();

  // Always grant access to this site to all extensions.
  DCHECK_GT(extension_ids_.size(), 0u);
  std::vector<const extensions::Extension*> extensions_to_run =
      GetExtensions(browser_->profile(), extension_ids_);
  extensions::SitePermissionsHelper(browser_->profile())
      .UpdateSiteAccess(
          extensions_to_run, web_contents,
          extensions::PermissionsManager::UserSiteAccess::kOnSite);

  // Show confirmation message, and disable the button, for a specific duration.
  std::optional<SkColor> color;
  SetHighlight(l10n_util::GetStringUTF16(
                   IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON_DISMISSED_TEXT),
               color);
  SetEnabled(false);

  base::TimeDelta collapse_duration = remove_confirmation_for_testing_
                                          ? base::Seconds(0)
                                          : kConfirmationDisplayDuration;
  // base::Unretained() below is safe because this view is tied to the
  // lifetime of `extensions_container_`.
  collapse_timer_.Start(
      FROM_HERE, collapse_duration,
      base::BindOnce(&ExtensionsContainer::CollapseConfirmation,
                     base::Unretained(extensions_container_)));

  base::RecordAction(base::UserMetricsAction(
      "Extensions.Toolbar.ExtensionsActivatedFromRequestAccessButton"));
  UMA_HISTOGRAM_COUNTS_100(
      "Extensions.Toolbar.ExtensionsActivatedFromRequestAccessButton",
      extension_ids_.size());
}

content::WebContents* ExtensionsRequestAccessButton::GetActiveWebContents()
    const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

BEGIN_METADATA(ExtensionsRequestAccessButton)
END_METADATA
