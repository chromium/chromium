// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/customize_chrome/side_panel_controller_views.h"

#include "base/functional/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer_helper.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "components/search/ntp_features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"

using SidePanelWebUIViewT_CustomizeChromeUI =
    SidePanelWebUIViewT<CustomizeChromeUI>;
BEGIN_TEMPLATE_METADATA(SidePanelWebUIViewT<CustomizeChromeUI>,
                        SidePanelWebUIViewT)
END_METADATA

namespace customize_chrome {

SidePanelControllerViews::SidePanelControllerViews(tabs::TabInterface& tab)
    : SidePanelControllerBase(tab) {}

SidePanelControllerViews::~SidePanelControllerViews() = default;

bool SidePanelControllerViews::ShouldEnableEditTheme(const GURL& url) const {
  Profile* const profile = tab_->GetProfile();
  return NewTabPageUI::IsNewTabPageOrigin(url) ||
         (base::FeatureList::IsEnabled(ntp_features::kNtpFooter) &&
          ntp_footer::IsExtensionNtp(url, profile));
}

void SidePanelControllerViews::OnEntryRegisteredForUrl(const GURL& url) {
  if (!customize_chrome_ui_) {
    return;
  }
  customize_chrome_ui_->AttachedTabStateUpdated(url);
  customize_chrome_ui_->UpdateThemeEditable(ShouldEnableEditTheme(url));
}

void SidePanelControllerViews::OpenSidePanel(
    SidePanelOpenTrigger trigger,
    std::optional<CustomizeChromeSection> section) {
  SidePanelControllerBase::OpenSidePanel(trigger, section);
  if (customize_chrome_ui_ && section.has_value()) {
    customize_chrome_ui_->ScrollToSection(section.value());
    section.reset();
  } else {
    section_ = section;
  }
}

SidePanelNativeView SidePanelControllerViews::CreateCustomizeChromeView(
    SidePanelEntryScope& scope) {
  auto customize_chrome_web_view =
      std::make_unique<SidePanelWebUIViewT<CustomizeChromeUI>>(
          scope, base::RepeatingClosure(), base::RepeatingClosure(),
          std::make_unique<WebUIContentsWrapperT<CustomizeChromeUI>>(
              GURL(chrome::kChromeUICustomizeChromeSidePanelURL),
              tab_->GetProfile(), IDS_SIDE_PANEL_CUSTOMIZE_CHROME_TITLE,
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

  // Immediately apply the tab's state to the customize chrome UI.
  content::NavigationEntry* entry =
      tab_->GetContents()->GetController().GetLastCommittedEntry();
  if (!entry) {
    entry = tab_->GetContents()->GetController().GetVisibleEntry();
  }
  const GURL& url = entry ? entry->GetURL() : GURL::EmptyGURL();
  customize_chrome_ui_->AttachedTabStateUpdated(url);
  customize_chrome_ui_->UpdateThemeEditable(ShouldEnableEditTheme(url));

  return customize_chrome_web_view;
}

}  // namespace customize_chrome
