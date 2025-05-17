// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/pwa_install_page_action.h"

#include <string>

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/l10n/l10n_util.h"

PwaInstallPageActionController::PwaInstallPageActionController(
    tabs::TabInterface& tab_interface)
    : tab_interface_(tab_interface) {
  content::WebContents* web_contents = tab_interface_->GetContents();
  if (web_contents) {
    Observe(web_contents);
    manager_ = webapps::AppBannerManager::FromWebContents(web_contents);
    // May not be present e.g. in incognito mode.
    if (manager_) {
      manager_->AddObserver(this);
    }
  }

  will_discard_contents_subscription_ =
      tab_interface_->RegisterWillDiscardContents(base::BindRepeating(
          &PwaInstallPageActionController::WillDiscardContents,
          base::Unretained(this)));
  will_deactivate_subscription_ = tab_interface_->RegisterWillDeactivate(
      base::BindRepeating(&PwaInstallPageActionController::WillDeactivate,
                          base::Unretained(this)));
}

void PwaInstallPageActionController::WillDiscardContents(
    tabs::TabInterface* tab_interface,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  if (manager_) {
    manager_->RemoveObserver(this);
  }
  if (new_contents) {
    manager_ = webapps::AppBannerManager::FromWebContents(new_contents);
    if (manager_) {
      manager_->AddObserver(this);
    }
  }
  Observe(new_contents);
}

void PwaInstallPageActionController::WillDeactivate(
    tabs::TabInterface* tab_interface) {
  UpdateVisibility();
}

PwaInstallPageActionController::~PwaInstallPageActionController() {
  if (manager_) {
    manager_->RemoveObserver(this);
  }
  Observe(nullptr);
}

void PwaInstallPageActionController::UpdateVisibility() {
  content::WebContents* web_contents = tab_interface_->GetContents();
  if (!web_contents) {
    return;
  }

  if (web_contents->IsCrashed()) {
    Hide();
    return;
  }

  if (!manager_) {
    return;
  }

  if (manager_->IsProbablyPromotableWebApp()) {
    Show(web_contents, manager_->MaybeConsumeInstallAnimation());
  } else {
    Hide();
  }
}

void PwaInstallPageActionController::Show(content::WebContents* web_contents,
                                          bool showChip) {
  // Controller responsible for all page actions
  page_actions::PageActionController& all_actions_controller =
      GetPageActionController();
  all_actions_controller.OverrideText(
      kActionInstallPwa,
      l10n_util::GetStringFUTF16(
          IDS_OMNIBOX_PWA_INSTALL_ICON_TOOLTIP,
          webapps::AppBannerManager::GetInstallableWebAppName(web_contents)));
  all_actions_controller.OverrideTooltip(
      kActionInstallPwa,
      l10n_util::GetStringFUTF16(
          IDS_OMNIBOX_PWA_INSTALL_ICON_TOOLTIP,
          webapps::AppBannerManager::GetInstallableWebAppName(web_contents)));
  all_actions_controller.Show(kActionInstallPwa);
  if (showChip) {
    all_actions_controller.ShowSuggestionChip(kActionInstallPwa);
  } else {
    all_actions_controller.HideSuggestionChip(kActionInstallPwa);
  }
}

void PwaInstallPageActionController::Hide() {
  // Controller responsible for all page actions
  page_actions::PageActionController& all_actions_controller =
      GetPageActionController();
  all_actions_controller.HideSuggestionChip(kActionInstallPwa);
  all_actions_controller.Hide(kActionInstallPwa);
  all_actions_controller.ClearOverrideText(kActionInstallPwa);
  all_actions_controller.ClearOverrideTooltip(kActionInstallPwa);
}

void PwaInstallPageActionController::OnInstallableWebAppStatusUpdated(
    webapps::InstallableWebAppCheckResult result,
    const std::optional<webapps::WebAppBannerData>& data) {
  UpdateVisibility();
}

void PwaInstallPageActionController::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  UpdateVisibility();
}

page_actions::PageActionController&
PwaInstallPageActionController::GetPageActionController() {
  page_actions::PageActionController* all_actions_controller =
      tab_interface_->GetTabFeatures()->page_action_controller();
  CHECK(all_actions_controller);
  return *all_actions_controller;
}
