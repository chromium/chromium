// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/customize_buttons/customize_buttons_handler.h"

#include <utility>

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/customize_chrome/side_panel_controller.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/pref_names.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_ui.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(CustomizeButtonsHandler,
                                      kCustomizeChromeButtonElementId);

CustomizeButtonsHandler::CustomizeButtonsHandler(
    mojo::PendingReceiver<customize_buttons::mojom::CustomizeButtonsHandler>
        pending_handler,
    mojo::PendingRemote<customize_buttons::mojom::CustomizeButtonsDocument>
        pending_page,
    content::WebUI* web_ui,
    tabs::TabInterface* tab_interface,
    std::unique_ptr<NewTabPageFeaturePromoHelper>
        customize_chrome_feature_promo_helper)
    : profile_(Profile::FromWebUI(web_ui)),
      web_ui_(web_ui),
      tab_interface_(tab_interface),
      feature_promo_helper_(std::move(customize_chrome_feature_promo_helper)),
      page_{std::move(pending_page)},
      receiver_{this, std::move(pending_handler)} {
  CHECK(web_ui_);
#if !BUILDFLAG(IS_ANDROID)
  CHECK(feature_promo_helper_);
#endif

  if (tab_interface_) {
    tab_subscriptions_.push_back(tab_interface_->RegisterWillDetach(
        base::BindRepeating(&CustomizeButtonsHandler::OnTabWillDetach,
                            weak_ptr_factory_.GetWeakPtr())));
  }

  SetCustomizeChromeEntryChangedCallback();
}

CustomizeButtonsHandler::~CustomizeButtonsHandler() = default;

customize_chrome::SidePanelController*
CustomizeButtonsHandler::SetCustomizeChromeEntryChangedCallback() {
  tabs::TabInterface* tab = GetActiveTab();
  if (!tab) {
    return nullptr;
  }

  auto* controller =
      customize_chrome::SidePanelController::Get(tab->GetUnownedUserDataHost());
  if (!controller) {
    return nullptr;
  }

  controller->SetEntryChangedCallback(base::BindRepeating(
      &CustomizeButtonsHandler::NotifyCustomizeChromeSidePanelVisibilityChanged,
      weak_ptr_factory_.GetWeakPtr()));
  return controller;
}

tabs::TabInterface* CustomizeButtonsHandler::GetActiveTab() {
  if (tab_interface_) {
    return tab_interface_;
  }

  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_ui_->GetWebContents());
  if (!browser_window_interface) {
    return nullptr;
  }

  auto* tab_list = TabListInterface::From(browser_window_interface);
  if (!tab_list) {
    return nullptr;
  }

  tabs::TabInterface* active_tab = tab_list->GetActiveTab();
  if (!active_tab) {
    // TODO(crbug.com/378475391): NTP or Footer should always load into a
    // WebContents owned by a TabModel. Remove this once NTP loading has been
    // restricted to browser tabs only.
    LOG(ERROR)
        << "NewTabPage or NewTabFooter loaded into a non-browser-tab context";
    return nullptr;
  }

  return active_tab;
}

void CustomizeButtonsHandler::NotifyCustomizeChromeSidePanelVisibilityChanged(
    bool is_open) {
  page_->SetCustomizeChromeSidePanelVisibility(is_open);
}

void CustomizeButtonsHandler::SetCustomizeChromeSidePanelVisible(
    bool visible,
    CustomizeChromeSection section,
    customize_buttons::mojom::SidePanelOpenTrigger trigger) {
  customize_chrome::SidePanelController*
      customize_chrome_side_panel_controller =
          SetCustomizeChromeEntryChangedCallback();
  CHECK(customize_chrome_side_panel_controller);

  if (!visible) {
    customize_chrome_side_panel_controller->CloseSidePanel();
    NotifyCustomizeChromeSidePanelVisibilityChanged(false);
    return;
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

  NotifyCustomizeChromeSidePanelVisibilityChanged(true);
  customize_chrome_side_panel_controller->OpenSidePanel(trigger_enum, section);

  // Record usage for customize chrome promo.
#if !BUILDFLAG(IS_ANDROID)
  if (feature_promo_helper_) {
    auto* tab = GetActiveTab();
    CHECK(tab);
    auto* contents = tab->GetContents();
    feature_promo_helper_->RecordPromoFeatureUsageAndClosePromo(
        feature_engagement::kIPHDesktopCustomizeChromeExperimentFeature,
        contents);
    feature_promo_helper_->RecordPromoFeatureUsageAndClosePromo(
        feature_engagement::kIPHDesktopCustomizeChromeAutoOpenFeature,
        contents);
  }
#endif
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

void CustomizeButtonsHandler::OnTabWillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  tab_interface_ = nullptr;
}
