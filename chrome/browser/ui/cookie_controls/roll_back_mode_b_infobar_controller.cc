// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cookie_controls/roll_back_mode_b_infobar_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cookie_controls/roll_back_mode_b_infobar_delegate.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

RollBackModeBInfoBarController::RollBackModeBInfoBarController(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

RollBackModeBInfoBarController::~RollBackModeBInfoBarController() = default;

void RollBackModeBInfoBarController::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (infobar_ || !profile ||
      !profile->GetPrefs()->GetBoolean(prefs::kShowRollbackUiModeB)) {
    return;
  }

  if (auto* infobar_manager =
          infobars::ContentInfoBarManager::FromWebContents(web_contents())) {
    infobar_ = RollBackModeBInfoBarDelegate::Create(infobar_manager);
    if (infobar_) {
      infobar_scoped_observation_.Observe(infobar_manager);
      profile->GetPrefs()->SetBoolean(prefs::kShowRollbackUiModeB, false);
    }
  }
}

void RollBackModeBInfoBarController::OnVisibilityChanged(
    content::Visibility visibility) {
  // Auto-dismiss `infobar_` if the tab is now hidden.
  if (infobar_ && visibility == content::Visibility::HIDDEN) {
    infobar_->RemoveSelf();
  }
}

void RollBackModeBInfoBarController::OnInfoBarRemoved(
    infobars::InfoBar* infobar,
    bool animate) {
  if (infobar == infobar_) {
    infobar_scoped_observation_.Reset();
    infobar_ = nullptr;
  }
}
