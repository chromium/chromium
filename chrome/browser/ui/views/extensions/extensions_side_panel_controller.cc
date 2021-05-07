// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_side_panel_controller.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/extension_id.h"
#include "net/base/url_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/webview/webview.h"

namespace {

// TODO(tluk): Figure out what our default width should be.
constexpr int kDefaultWidth = 450;

const char kPanelActiveKey[] = "active";
const char kPanelActivatableKey[] = "activatable";
const char kPanelWidth[] = "width";

const char kPanelTrueValue[] = "true";

}  // namespace

ExtensionsSidePanelController::ExtensionsSidePanelController(
    SidePanel* side_panel,
    BrowserView* browser_view)
    : extension_id_(features::kExtensionsSidePanelId.Get()),
      side_panel_(side_panel),
      browser_view_(browser_view),
      web_view_(side_panel_->AddChildView(
          std::make_unique<views::WebView>(browser_view_->GetProfile()))) {
  DCHECK(base::FeatureList::IsEnabled(features::kExtensionsSidePanel));

  side_panel_->SetVisible(false);
  side_panel_->SetPanelWidth(kDefaultWidth);
  Observe(web_view_->GetWebContents());

  // Enable the hosted WebContents to leverage extensions APIs.
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_view_->GetWebContents());

  // Set the side panel as type kExtensionPopup given this most closely
  // represents how we intend to use the extension for the side panel.
  // TODO(tluk): Consider creating a new type specifically for the extension
  // side panel.
  extensions::SetViewType(web_view_->GetWebContents(),
                          extensions::mojom::ViewType::kExtensionPopup);

  if (const extensions::Extension* extension = GetExtension())
    web_view_->LoadInitialURL(extension->GetResourceURL("side_panel.html"));
}

ExtensionsSidePanelController::~ExtensionsSidePanelController() = default;

std::unique_ptr<ToolbarButton>
ExtensionsSidePanelController::CreateToolbarButton() {
  auto toolbar_button = std::make_unique<ToolbarButton>();
  // TODO(tluk): Update this to use the icon from the extension.
  toolbar_button->SetVectorIcon(kWebIcon);
  toolbar_button->SetCallback(base::BindRepeating(
      &ExtensionsSidePanelController::SidePanelButtonPressed,
      base::Unretained(this)));
  return toolbar_button;
}

// The extension host uses URL params to control various properties of the side
// panel such as visibility and width. Check for these params and adjust the
// side panel accordingly.
void ExtensionsSidePanelController::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  const GURL& url = load_details.entry->GetURL();
  std::string value;

  // TODO(crbug.com/1203063): The side panel contents should not be active when
  // the panel is not active. This could lead to issues wrt extension lifetime
  // and resource consumption. Remove or fix this after the experiment has
  // concluded.
  if (net::GetValueForKeyInQuery(url, kPanelActiveKey, &value))
    side_panel_->SetVisible(value == kPanelTrueValue);

  if (net::GetValueForKeyInQuery(url, kPanelWidth, &value)) {
    unsigned int width = 0;
    base::StringToUint(value, &width);
    side_panel_->SetPanelWidth(width);
  }

  if (net::GetValueForKeyInQuery(url, kPanelActivatableKey, &value)) {
    auto* left_side_panel_button =
        browser_view_->toolbar()->left_side_panel_button();
    DCHECK(left_side_panel_button);
    left_side_panel_button->SetEnabled(value == kPanelTrueValue);
  }
}

const extensions::Extension* ExtensionsSidePanelController::GetExtension() {
  return extensions::ExtensionRegistry::Get(browser_view_->GetProfile())
      ->enabled_extensions()
      .GetByID(extension_id_);
}

void ExtensionsSidePanelController::SidePanelButtonPressed() {
  const auto* extension = GetExtension();
  extensions::ExtensionAction action(
      *extension, extensions::ActionInfo(extensions::ActionInfo::TYPE_BROWSER));
  auto* web_contents =
      browser_view_->browser()->tab_strip_model()->GetActiveWebContents();
  extensions::ExtensionActionAPI::Get(browser_view_->GetProfile())
      ->DispatchExtensionActionClicked(action, web_contents, extension);
}
