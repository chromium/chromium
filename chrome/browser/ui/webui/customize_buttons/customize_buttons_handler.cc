// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/customize_buttons/customize_buttons_handler.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/pref_names.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"

CustomizeButtonsHandler::CustomizeButtonsHandler(
    mojo::PendingReceiver<customize_buttons::mojom::CustomizeButtonsHandler>
        pending_handler,
    mojo::PendingRemote<customize_buttons::mojom::CustomizeButtonsDocument>
        pending_page,
    Profile* profile,
    content::WebContents* web_contents,
    std::unique_ptr<NewTabPageFeaturePromoHelper>
        customize_chrome_feature_promo_helper)
    : profile_(profile),
      web_contents_(web_contents),
      feature_promo_helper_(std::move(customize_chrome_feature_promo_helper)),
      tab_changed_subscription_(webui::RegisterTabInterfaceChanged(
          web_contents,
          base::BindRepeating(&CustomizeButtonsHandler::OnTabInterfaceChanged,
                              base::Unretained(this)))),
      page_{std::move(pending_page)},
      receiver_{this, std::move(pending_handler)} {
  CHECK(web_contents_);
  CHECK(feature_promo_helper_);
  OnTabInterfaceChanged();
}

CustomizeButtonsHandler::~CustomizeButtonsHandler() = default;

void CustomizeButtonsHandler::OnTabInterfaceChanged() {
  tabs::TabInterface* tab_interface =
      webui::GetTabInterface(web_contents_.get());
  if (!tab_interface) {
    // TODO(crbug.com/378475391): NTP or Footer should always load into a
    // WebContents owned by a TabModel. Remove this once NTP loading has been
    // restricted to browser tabs only.
    LOG(ERROR)
        << "NewTabPage or NewTabFooter loaded into a non-browser-tab context";

    // Reset any composed tab features here.
    SetCustomizeChromeSidePanelController(nullptr);
    return;
  }

  SetCustomizeChromeSidePanelController(
      tab_interface->GetTabFeatures()
          ->customize_chrome_side_panel_controller());
}

void CustomizeButtonsHandler::SetCustomizeChromeSidePanelController(
    customize_chrome::SidePanelController* side_panel_controller) {
  customize_chrome_side_panel_controller_ = side_panel_controller;

  if (customize_chrome_side_panel_controller_) {
    page_->SetCustomizeChromeSidePanelVisibility(
        customize_chrome_side_panel_controller_
            ->IsCustomizeChromeEntryShowing());
    customize_chrome_side_panel_controller_->SetEntryChangedCallback(
        base::BindRepeating(&CustomizeButtonsHandler::
                                NotifyCustomizeChromeSidePanelVisibilityChanged,
                            weak_ptr_factory_.GetWeakPtr()));
  } else {
    page_->SetCustomizeChromeSidePanelVisibility(false);
  }
}

void CustomizeButtonsHandler::NotifyCustomizeChromeSidePanelVisibilityChanged(
    bool is_open) {
  page_->SetCustomizeChromeSidePanelVisibility(is_open);
}

void CustomizeButtonsHandler::SetCustomizeChromeSidePanelVisible(
    bool visible,
    customize_buttons::mojom::CustomizeChromeSection section,
    customize_buttons::mojom::SidePanelOpenTrigger trigger) {
  CHECK(customize_chrome_side_panel_controller_);
  if (!visible) {
    customize_chrome_side_panel_controller_->CloseSidePanel();
    return;
  }

  CustomizeChromeSection section_enum;
  // TODO(crbug.com/419081665) Dedupe CustomizeChromeSection mojom enums.
  switch (section) {
    case customize_buttons::mojom::CustomizeChromeSection::kUnspecified:
      section_enum = CustomizeChromeSection::kUnspecified;
      break;
    case customize_buttons::mojom::CustomizeChromeSection::kAppearance:
      section_enum = CustomizeChromeSection::kAppearance;
      break;
    case customize_buttons::mojom::CustomizeChromeSection::kShortcuts:
      section_enum = CustomizeChromeSection::kShortcuts;
      break;
    case customize_buttons::mojom::CustomizeChromeSection::kModules:
      section_enum = CustomizeChromeSection::kModules;
      break;
    case customize_buttons::mojom::CustomizeChromeSection::kWallpaperSearch:
      section_enum = CustomizeChromeSection::kWallpaperSearch;
      break;
    case customize_buttons::mojom::CustomizeChromeSection::kToolbar:
      section_enum = CustomizeChromeSection::kToolbar;
      break;
  }

  SidePanelOpenTrigger trigger_enum;
  switch (trigger) {
    case customize_buttons::mojom::SidePanelOpenTrigger::kNewTabPage:
      trigger_enum = SidePanelOpenTrigger::kNewTabPage;
      break;
    case customize_buttons::mojom::SidePanelOpenTrigger::kNewTabFooter:
      trigger_enum = SidePanelOpenTrigger::kNewTabFooter;
      break;
  }

  customize_chrome_side_panel_controller_->OpenSidePanel(trigger_enum,
                                                         section_enum);

  // Record usage for customize chrome promo.
  auto* tab = web_contents_.get();
  feature_promo_helper_->RecordPromoFeatureUsageAndClosePromo(
      feature_engagement::kIPHDesktopCustomizeChromeRefreshFeature, tab);
  feature_promo_helper_->RecordPromoFeatureUsageAndClosePromo(
      feature_engagement::kIPHDesktopCustomizeChromeFeature, tab);
}

void CustomizeButtonsHandler::IncrementCustomizeChromeButtonOpenCount() {
  CHECK(profile_);
  CHECK(profile_->GetPrefs());
  profile_->GetPrefs()->SetInteger(
      prefs::kNtpCustomizeChromeButtonOpenCount,
      profile_->GetPrefs()->GetInteger(
          prefs::kNtpCustomizeChromeButtonOpenCount) +
          1);
}

void CustomizeButtonsHandler::IncrementWallpaperSearchButtonShownCount() {
  const auto shown_count = profile_->GetPrefs()->GetInteger(
      prefs::kNtpWallpaperSearchButtonShownCount);
  profile_->GetPrefs()->SetInteger(prefs::kNtpWallpaperSearchButtonShownCount,
                                   shown_count + 1);
}

void CustomizeButtonsHandler::SetCustomizeChromeSidePanelControllerForTesting(
    customize_chrome::SidePanelController* side_panel_controller) {
  SetCustomizeChromeSidePanelController(side_panel_controller);
}
