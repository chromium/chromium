// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/webview/webview_side_panel_coordinator.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"

WebViewSidePanelCoordinator::WebViewSidePanelCoordinator(Browser* browser)
    : BrowserUserData<WebViewSidePanelCoordinator>(*browser) {}
WebViewSidePanelCoordinator::~WebViewSidePanelCoordinator() = default;

void WebViewSidePanelCoordinator::CreateAndRegisterEntry(
    SidePanelRegistry* registry) {
  registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kWebView,
      l10n_util::GetStringUTF16(IDS_SIDEBAR_WEBVIEW_TITLE),
      ui::ImageModel::FromVectorIcon(omnibox::kDinoIcon, ui::kColorIcon),
      base::BindRepeating(&WebViewSidePanelCoordinator::CreateView,
                          base::Unretained(this))));
}

std::unique_ptr<views::View> WebViewSidePanelCoordinator::CreateView() {
  auto* layout_provider = views::LayoutProvider::Get();
  auto margin = gfx::Insets::VH(
      layout_provider->GetDistanceMetric(
          ChromeDistanceMetric::DISTANCE_RELATED_CONTROL_VERTICAL_SMALL),
      0);

  auto view = std::make_unique<views::View>();
  view->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetInteriorMargin(margin)
      .SetDefault(views::kMarginsKey, margin);

  auto location = std::make_unique<views::Textfield>();
  location->GetViewAccessibility().OverrideName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_SIDEBAR_WEBVIEW_LOCATION_BAR));
  location->SetController(this);
  location_ = view->AddChildView(std::move(location));

  auto webview = std::make_unique<views::WebView>(GetBrowser().profile());
  webview->LoadInitialURL(GURL("chrome://dino"));
  Observe(webview->GetWebContents());
  webview->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded));
  webview_ = view->AddChildView(std::move(webview));

  return view;
}

void WebViewSidePanelCoordinator::PrimaryPageChanged(content::Page& page) {
  location_->SetText(base::UTF8ToUTF16(
      webview_->GetWebContents()->GetVisibleURL().possibly_invalid_spec()));
}

bool WebViewSidePanelCoordinator::HandleKeyEvent(
    views::Textfield* sender,
    const ui::KeyEvent& key_event) {
  if (key_event.type() == ui::ET_KEY_PRESSED &&
      key_event.key_code() == ui::VKEY_RETURN) {
    webview_->GetWebContents()->GetController().LoadURL(
        GURL(sender->GetText()), {}, ui::PageTransition::PAGE_TRANSITION_TYPED,
        {});
    return true;
  }
  return false;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebViewSidePanelCoordinator);
