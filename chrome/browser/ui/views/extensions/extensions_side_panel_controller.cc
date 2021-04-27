// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_side_panel_controller.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/side_panel.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension_id.h"
#include "net/base/url_util.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace {

// TODO(tluk): Figure out what our default width should be.
constexpr int kDefaultWidth = 450;

const char kPanelActiveKey[] = "active";
const char kPanelActiveValue[] = "true";

const char kPanelWidth[] = "width";

}  // namespace

ExtensionsSidePanelController::ExtensionsSidePanelController(
    SidePanel* side_panel,
    content::BrowserContext* browser_context)
    : side_panel_(side_panel),
      web_view_(side_panel_->AddChildView(
          std::make_unique<views::WebView>(browser_context))) {
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

  const extensions::ExtensionId& extension_id =
      features::kExtensionsSidePanelId.Get();
  if (const extensions::Extension* extension =
          extensions::ExtensionRegistry::Get(browser_context)
              ->enabled_extensions()
              .GetByID(extension_id)) {
    web_view_->LoadInitialURL(extension->GetResourceURL("side_panel.html"));
  }
}

ExtensionsSidePanelController::~ExtensionsSidePanelController() = default;

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
    side_panel_->SetVisible(value == kPanelActiveValue);

  if (net::GetValueForKeyInQuery(url, kPanelWidth, &value)) {
    unsigned int width = 0;
    base::StringToUint(value, &width);
    side_panel_->SetPanelWidth(width);
  }
}
