// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_page_handler.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/new_tab_page/ntp_pref_names.h"
#include "components/prefs/pref_service.h"

CustomizeChromePageHandler::CustomizeChromePageHandler(
    mojo::PendingReceiver<side_panel::mojom::CustomizeChromePageHandler>
        receiver,
    Profile* profile)
    : receiver_(this, std::move(receiver)), profile_(profile) {}

CustomizeChromePageHandler::~CustomizeChromePageHandler() = default;

void CustomizeChromePageHandler::SetMostVisitedSettings(
    bool custom_links_enabled,
    bool visible) {
  if (IsShortcutsVisible() != visible)
    profile_->GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, visible);

  if (IsCustomLinksEnabled() != custom_links_enabled) {
    profile_->GetPrefs()->SetBoolean(ntp_prefs::kNtpUseMostVisitedTiles,
                                     !custom_links_enabled);
  }
}

void CustomizeChromePageHandler::GetMostVisitedSettings(
    GetMostVisitedSettingsCallback callback) {
  std::move(callback).Run(IsCustomLinksEnabled(), IsShortcutsVisible());
}

bool CustomizeChromePageHandler::IsCustomLinksEnabled() const {
  return !profile_->GetPrefs()->GetBoolean(ntp_prefs::kNtpUseMostVisitedTiles);
}

bool CustomizeChromePageHandler::IsShortcutsVisible() const {
  return profile_->GetPrefs()->GetBoolean(ntp_prefs::kNtpShortcutsVisible);
}
