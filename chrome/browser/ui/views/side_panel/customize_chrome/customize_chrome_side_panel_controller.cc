// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/customize_chrome/customize_chrome_side_panel_controller.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/side_panel/customize_chrome/customize_chrome_tab_helper.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry_observer.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/view_class_properties.h"

using SidePanelWebUIViewT_CustomizeChromeUI =
    SidePanelWebUIViewT<CustomizeChromeUI>;
BEGIN_TEMPLATE_METADATA(SidePanelWebUIViewT<CustomizeChromeUI>,
                        SidePanelWebUIViewT)
END_METADATA

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
      ui::ImageModel::FromVectorIcon(features::IsChromeRefresh2023()
                                         ? vector_icons::kEditChromeRefreshIcon
                                         : vector_icons::kEditIcon,
                                     ui::kColorIcon, icon_size),
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

void CustomizeChromeSidePanelController::SetCustomizeChromeSidePanelVisible(
    bool visible,
    CustomizeChromeSection section) {
  auto* side_panel_ui = GetSidePanelUI();
  if (!side_panel_ui) {
    return;
  }
  DCHECK(IsCustomizeChromeEntryAvailable());
  if (visible) {
    side_panel_ui->Show(SidePanelEntry::Id::kCustomizeChrome);
    if (customize_chrome_ui_) {
      customize_chrome_ui_->ScrollToSection(section);
      section_.reset();
    } else {
      section_ = section;
    }
  } else {
    side_panel_ui->Close();
  }
}

bool CustomizeChromeSidePanelController::IsCustomizeChromeEntryShowing() const {
  auto* side_panel_ui = GetSidePanelUI();
  return side_panel_ui && side_panel_ui->IsSidePanelShowing() &&
         (side_panel_ui->GetCurrentEntryId() ==
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
  customize_chrome_web_view->SetProperty(
      views::kElementIdentifierKey, kCustomizeChromeSidePanelWebViewElementId);
  customize_chrome_web_view->ShowUI();
  customize_chrome_ui_ = customize_chrome_web_view->contents_wrapper()
                             ->GetWebUIController()
                             ->GetWeakPtr();
  if (section_.has_value()) {
    customize_chrome_ui_->ScrollToSection(*section_);
    section_.reset();
  }
  return customize_chrome_web_view;
}

SidePanelUI* CustomizeChromeSidePanelController::GetSidePanelUI() const {
  auto* browser = chrome::FindBrowserWithTab(web_contents_);
  return browser ? SidePanelUI::GetSidePanelUIForBrowser(browser) : nullptr;
}
