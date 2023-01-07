// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/customize_chrome/customize_chrome_side_panel_controller.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/side_panel/customize_chrome/customize_chrome_tab_helper.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry_observer.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/l10n/l10n_util.h"

CustomizeChromeSidePanelController::CustomizeChromeSidePanelController(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

CustomizeChromeSidePanelController::~CustomizeChromeSidePanelController() =
    default;

void CustomizeChromeSidePanelController::CreateAndRegisterEntry() {
  const int icon_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      ChromeDistanceMetric::DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE);
  auto* registry = SidePanelRegistry::Get(web_contents_);

  if (!registry)
    return;

  auto entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kCustomizeChrome,
      l10n_util::GetStringUTF16(IDS_SIDE_PANEL_CUSTOMIZE_CHROME_TITLE),
      ui::ImageModel::FromVectorIcon(vector_icons::kEditIcon, ui::kColorIcon,
                                     icon_size),
      base::BindRepeating(
          &CustomizeChromeSidePanelController::CreateCustomizeChromeWebView,
          base::Unretained(this)));
  entry->AddObserver(this);
  registry->Register(std::move(entry));
}

void CustomizeChromeSidePanelController::DeregisterEntry() {
  auto* registry = SidePanelRegistry::Get(web_contents_);

  if (!registry)
    return;

  if (auto* current_entry = registry->GetEntryForKey(
          SidePanelEntry::Key(SidePanelEntry::Id::kCustomizeChrome))) {
    current_entry->RemoveObserver(this);
  }

  registry->Deregister(
      SidePanelEntry::Key(SidePanelEntry::Id::kCustomizeChrome));
}

void CustomizeChromeSidePanelController::ShowCustomizeChromeSidePanel() {
  auto* browser_view = GetBrowserView();
  if (!browser_view)
    return;
  DCHECK(IsCustomizeChromeEntryAvailable());
  browser_view->side_panel_coordinator()->Show(
      SidePanelEntry::Id::kCustomizeChrome);
}

bool CustomizeChromeSidePanelController::IsCustomizeChromeEntryShowing() const {
  auto* browser_view = GetBrowserView();
  if (!browser_view)
    return false;
  auto* side_panel_coordinator = browser_view->side_panel_coordinator();
  return side_panel_coordinator->IsSidePanelShowing() &&
         (side_panel_coordinator->GetCurrentEntryId() ==
          SidePanelEntry::Id::kCustomizeChrome);
}

bool CustomizeChromeSidePanelController::IsCustomizeChromeEntryAvailable()
    const {
  auto* registry = SidePanelRegistry::Get(web_contents_);
  return registry ? (registry->GetEntryForKey(SidePanelEntry::Key(
                         SidePanelEntry::Id::kCustomizeChrome)) != nullptr)
                  : false;
}

void CustomizeChromeSidePanelController::OnEntryShown(SidePanelEntry* entry) {
  auto* customize_chrome_tab_helper =
      CustomizeChromeTabHelper::FromWebContents(web_contents_);
  customize_chrome_tab_helper->EntryStateChanged(true);
}

void CustomizeChromeSidePanelController::OnEntryHidden(SidePanelEntry* entry) {
  auto* customize_chrome_tab_helper =
      CustomizeChromeTabHelper::FromWebContents(web_contents_);
  customize_chrome_tab_helper->EntryStateChanged(false);
}

std::unique_ptr<views::View>
CustomizeChromeSidePanelController::CreateCustomizeChromeWebView() {
  auto customize_chrome_web_view =
      std::make_unique<SidePanelWebUIViewT<CustomizeChromeUI>>(
          base::RepeatingClosure(), base::RepeatingClosure(),
          std::make_unique<BubbleContentsWrapperT<CustomizeChromeUI>>(
              GURL(chrome::kChromeUICustomizeChromeSidePanelURL),
              Profile::FromBrowserContext(web_contents_->GetBrowserContext()),
              IDS_SIDE_PANEL_CUSTOMIZE_CHROME_TITLE,
              /*webui_resizes_host=*/false,
              /*esc_closes_ui=*/false));
  customize_chrome_web_view->ShowUI();
  return customize_chrome_web_view;
}

BrowserView* CustomizeChromeSidePanelController::GetBrowserView() const {
  auto* browser = chrome::FindBrowserWithWebContents(web_contents_);
  return browser ? BrowserView::GetBrowserViewForBrowser(browser) : nullptr;
}
