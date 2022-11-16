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
    const std::vector<std::unique_ptr<ToolbarActionViewController>>& actions,
    extensions::PermissionsManager::UserSiteSetting site_setting,
    content::WebContents* current_web_contents) {
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

void ExtensionsToolbarControls::UpdateRequestAccessButton(
    const std::vector<std::unique_ptr<ToolbarActionViewController>>& actions,
    extensions::PermissionsManager::UserSiteSetting site_setting,
    content::WebContents* web_contents) {
  // User site settings takes precedence over extension site access. If the user
  // has allowed or blocked all extensions, individual extensions cannot grant
  // access to the page and therefore the request access button is not
  // displayed.
  if (site_setting == extensions::PermissionsManager::UserSiteSetting::
                          kGrantAllExtensions ||
      site_setting == extensions::PermissionsManager::UserSiteSetting::
                          kBlockAllExtensions) {
    request_access_button_->SetVisible(false);
    return;
  }

  // Request access button is displayed if any extension requests access.
  std::vector<ToolbarActionViewController*> extensions_requesting_access;
  for (const auto& action : actions) {
    if (action->IsRequestingSiteAccess(web_contents))
      extensions_requesting_access.push_back(action.get());
  }
  if (extensions_requesting_access.empty()) {
    request_access_button_->SetVisible(false);
  } else {
    // TODO(crbug.com/1239772): Update icons, based on the number of extensions
    // requesting access, once multiple icons in button is supported. Since we
    // will need to access the extension information, this method may receive
    // actions instead of actions count. For now, just show the number of
    // actions.
    request_access_button_->UpdateExtensionsRequestingAccess(
        extensions_requesting_access);
    request_access_button_->SetVisible(true);
  }
}

BEGIN_METADATA(ExtensionsToolbarControls, ToolbarIconContainerView)
END_METADATA
