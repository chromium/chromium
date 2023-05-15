// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_controls.h"

#include <memory>

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_request_access_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/permissions_manager.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/background.h"

ExtensionsToolbarControls::ExtensionsToolbarControls(
    std::unique_ptr<ExtensionsToolbarButton> extensions_button,
    std::unique_ptr<ExtensionsRequestAccessButton> request_access_button)
    : ToolbarIconContainerView(/*uses_highlight=*/true),
      request_access_button_(AddChildView(std::move(request_access_button))),
      extensions_button_(extensions_button.get()) {
  request_access_button_->SetVisible(false);
  // TODO(emiliapaz): Consider changing AddMainItem() to receive a unique_ptr.
  AddMainItem(extensions_button.release());
}

ExtensionsToolbarControls::~ExtensionsToolbarControls() = default;

void ExtensionsToolbarControls::UpdateAllIcons() {}

void ExtensionsToolbarControls::UpdateControls(
    bool is_restricted_url,
    const std::vector<std::unique_ptr<ToolbarActionViewController>>& actions,
    extensions::PermissionsManager::UserSiteSetting site_setting,
    content::WebContents* current_web_contents) {
  UpdateExtensionsButton(actions, site_setting, current_web_contents,
                         is_restricted_url);
  UpdateRequestAccessButton(actions, site_setting, current_web_contents);

  // Display background only when multiple buttons are visible. Since
  // the extensions button is always visible, check if the request access
  // button is too.
  SetBackground(request_access_button_->GetVisible()
                    ? views::CreateThemedRoundedRectBackground(
                          kColorExtensionsToolbarControlsBackground,
                          extensions_button_->GetPreferredSize().height())
                    : nullptr);

  // Resets the layout since layout animation does not handle host view
  // visibility changing. This should be called after any visibility changes.
  GetAnimatingLayoutManager()->ResetLayout();
}

void ExtensionsToolbarControls::UpdateExtensionsButton(
    const std::vector<std::unique_ptr<ToolbarActionViewController>>& actions,
    extensions::PermissionsManager::UserSiteSetting site_setting,
    content::WebContents* web_contents,
    bool is_restricted_url) {
  ExtensionsToolbarButton::State extensions_button_state =
      ExtensionsToolbarButton::State::kDefault;

  if (is_restricted_url || site_setting ==
                               extensions::PermissionsManager::UserSiteSetting::
                                   kBlockAllExtensions) {
    extensions_button_state =
        ExtensionsToolbarButton::State::kAllExtensionsBlocked;
  } else if (ExtensionActionViewController::AnyActionHasCurrentSiteAccess(
                 actions, web_contents)) {
    extensions_button_state =
        ExtensionsToolbarButton::State::kAnyExtensionHasAccess;
  }

  extensions_button_->UpdateState(extensions_button_state);
}

void ExtensionsToolbarControls::UpdateRequestAccessButton(
    const std::vector<std::unique_ptr<ToolbarActionViewController>>& actions,
    extensions::PermissionsManager::UserSiteSetting site_setting,
    content::WebContents* web_contents) {
  // Extensions are included in the request access button only when the site
  // allows customizing site access by extension, and when the extension
  // itself can show access requests in the toolbar.
  std::vector<extensions::ExtensionId> extensions;
  if (site_setting ==
      extensions::PermissionsManager::UserSiteSetting::kCustomizeByExtension) {
    for (const auto& action : actions) {
      if (action->ShouldShowSiteAccessRequestInToolbar(web_contents)) {
        extensions.push_back(action->GetId());
      }
    }
  }

  request_access_button_->Update(extensions);
}

BEGIN_METADATA(ExtensionsToolbarControls, ToolbarIconContainerView)
END_METADATA
