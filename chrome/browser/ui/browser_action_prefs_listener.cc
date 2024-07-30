// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_action_prefs_listener.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"

BrowserActionPrefsListener::BrowserActionPrefsListener(Browser& browser)
    : browser_(browser) {
  if (auto* profile_prefs = browser_->profile()->GetPrefs()) {
    profile_pref_registrar_.Init(profile_prefs);
#if !BUILDFLAG(IS_CHROMEOS)
    profile_pref_registrar_.Add(
        prefs::kDesktopSharingHubEnabled,
        base::BindRepeating(
            &BrowserActionPrefsListener::UpdateActionsForSharingHubPolicy,
            base::Unretained(this)));
#endif  // !BUILDFLAG(IS_CHROMEOS)
  }
  if (auto* local_prefs = g_browser_process->local_state()) {
    local_pref_registrar_.Init(local_prefs);
    local_pref_registrar_.Add(
        prefs::kQRCodeGeneratorEnabled,
        base::BindRepeating(&BrowserActionPrefsListener::
                                UpdateQRCodeGeneratorActionEnabledState,
                            base::Unretained(this)));
  }
}

BrowserActionPrefsListener::~BrowserActionPrefsListener() {
  profile_pref_registrar_.RemoveAll();
  local_pref_registrar_.RemoveAll();
}

void BrowserActionPrefsListener::UpdateActionsForSharingHubPolicy() {
#if !BUILDFLAG(IS_CHROMEOS)
  // Update the visibility of the QR code generator, send tab to self, and copy
  // link actions based on the sharing hub policy. This matches the fact that
  // these actions' visibility in the app menu.
  bool sharing_enabled = browser_->profile()->GetPrefs()->GetBoolean(
      prefs::kDesktopSharingHubEnabled);

  auto update_action_visibility =
      [this, &sharing_enabled](actions::ActionId action_id) {
        if (auto* action_item = actions::ActionManager::Get().FindAction(
                action_id, browser_->browser_actions()->root_action_item())) {
          action_item->SetVisible(sharing_enabled);
        }
      };
  update_action_visibility(kActionQrCodeGenerator);
  update_action_visibility(kActionSendTabToSelf);
  update_action_visibility(kActionCopyUrl);
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

void BrowserActionPrefsListener::UpdateQRCodeGeneratorActionEnabledState() {
  bool qr_code_generator_enabled = g_browser_process->local_state()->GetBoolean(
      prefs::kQRCodeGeneratorEnabled);
  if (auto* qr_code_action_item = actions::ActionManager::Get().FindAction(
          kActionQrCodeGenerator,
          browser_->browser_actions()->root_action_item())) {
    qr_code_action_item->SetEnabled(qr_code_generator_enabled);
  }
}
