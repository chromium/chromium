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
#include "chrome/grit/generated_resources.h"
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

const char kSidePanelResourceName[] = "side_panel.html";
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
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser_view->GetProfile());
  registry_observation_.Observe(registry);

  side_panel_->SetVisible(false);
  side_panel_->SetPanelWidth(kDefaultWidth);

  if (const extensions::Extension* extension = GetExtension()) {
    SetNewWebContents();
    web_view_->LoadInitialURL(
        extension->GetResourceURL(kSidePanelResourceName));
  }
}

ExtensionsSidePanelController::~ExtensionsSidePanelController() = default;

std::unique_ptr<ToolbarButton>
ExtensionsSidePanelController::CreateToolbarButton() {
  auto toolbar_button = std::make_unique<ToolbarButton>();
  toolbar_button->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_LEFT_ALIGNED_SIDE_PANEL_BUTTON));
  toolbar_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_LEFT_ALIGNED_SIDE_PANEL_BUTTON));
  // TODO(tluk): Update this to use the icon from the extension.
  toolbar_button->SetVectorIcon(kWebIcon);
  toolbar_button->SetCallback(base::BindRepeating(
      &ExtensionsSidePanelController::SidePanelButtonPressed,
      base::Unretained(this)));
  toolbar_button->SetVisible(!!GetExtension());
  toolbar_button->SetEnabled(false);
  return toolbar_button;
}

void ExtensionsSidePanelController::ResetWebContents() {
  if (!web_view_->web_contents() && !web_contents_)
    return;

  DCHECK_EQ(web_view_->web_contents(), web_contents_.get());
  Observe(nullptr);
  web_view_->SetWebContents(nullptr);
  web_contents_.reset();
}

void ExtensionsSidePanelController::SetNewWebContents() {
  if (web_view_->web_contents())
    ResetWebContents();

  DCHECK(!web_view_->web_contents());
  DCHECK(!web_contents_);
  web_contents_ = content::WebContents::Create(
      content::WebContents::CreateParams(browser_view_->GetProfile()));

  // Enable the hosted WebContents to leverage extensions APIs.
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents_.get());

  // Set the side panel as type kExtensionPopup given this most closely
  // represents how we intend to use the extension for the side panel.
  // TODO(tluk): Consider creating a new type specifically for the extension
  // side panel.
  extensions::SetViewType(web_contents_.get(),
                          extensions::mojom::ViewType::kExtensionPopup);

  Observe(web_contents_.get());
  web_contents_->SetDelegate(this);
  web_view_->SetWebContents(web_contents_.get());
}

content::WebContents* ExtensionsSidePanelController::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  return browser_view_->browser()->OpenURL(params);
}

void ExtensionsSidePanelController::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  if (extension->id() == extension_id_) {
    SetNewWebContents();
    web_view_->LoadInitialURL(
        extension->GetResourceURL(kSidePanelResourceName));
    browser_view_->toolbar()->left_side_panel_button()->SetVisible(true);
  }
}

void ExtensionsSidePanelController::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  if (extension->id() == extension_id_) {
    ResetWebContents();
    browser_view_->toolbar()->left_side_panel_button()->SetVisible(false);
    side_panel_->SetVisible(false);
  }
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
