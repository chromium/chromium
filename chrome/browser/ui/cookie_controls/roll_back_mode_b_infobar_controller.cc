// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cookie_controls/roll_back_mode_b_infobar_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/cookie_controls/roll_back_mode_b_infobar_delegate.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

RollBackModeBInfoBarController::RollBackModeBInfoBarController(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

RollBackModeBInfoBarController::~RollBackModeBInfoBarController() = default;

void RollBackModeBInfoBarController::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  // Offboard here iff the user *did not* block 3PCs in Mode B and therefore
  // will not need their 3PC blocking state updated (setting 3PC blocking
  // state here creates a startup race condition with local pref resolution).
  if (profile && navigation_handle->IsInPrimaryMainFrame() &&
      !profile->GetPrefs()->GetBoolean(prefs::kBlockAll3pcToggleEnabled)) {
    privacy_sandbox::MaybeSetRollbackPrefsModeB(
        SyncServiceFactory::GetForProfile(profile), profile->GetPrefs());
  }
}

void RollBackModeBInfoBarController::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (infobar_ || !profile) {
    return;
  }
  // Offboard here iff the user *did* block 3PCs in Mode B. There's no
  // material difference between doing this here and on DidStartNavigation, as
  // the user will continue having 3PCs blocked, and this avoids the race.
  if (profile->GetPrefs()->GetBoolean(prefs::kBlockAll3pcToggleEnabled)) {
    privacy_sandbox::MaybeSetRollbackPrefsModeB(
        SyncServiceFactory::GetForProfile(profile), profile->GetPrefs());
    return;
  }
  if (!navigation_handle->HasCommitted() ||
      !navigation_handle->IsInPrimaryMainFrame() ||
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
