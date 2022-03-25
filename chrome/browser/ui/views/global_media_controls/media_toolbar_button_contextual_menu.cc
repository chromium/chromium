// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_contextual_menu.h"

#include "base/strings/strcat.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/media_router/common/pref_names.h"
#include "components/prefs/pref_service.h"

std::unique_ptr<MediaToolbarButtonContextualMenu>
MediaToolbarButtonContextualMenu::Create(Browser* browser) {
  if (media_router::GlobalMediaControlsCastStartStopEnabled(
          browser->profile())) {
    return std::make_unique<MediaToolbarButtonContextualMenu>(browser);
  }
  return nullptr;
}

MediaToolbarButtonContextualMenu::MediaToolbarButtonContextualMenu(
    Browser* browser)
    : browser_(browser) {}

MediaToolbarButtonContextualMenu::~MediaToolbarButtonContextualMenu() = default;

std::unique_ptr<ui::SimpleMenuModel>
MediaToolbarButtonContextualMenu::CreateMenuModel() {
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(this);
  menu_model->AddCheckItemWithStringId(
      IDC_MEDIA_TOOLBAR_CONTEXT_SHOW_OTHER_SESSIONS,
      IDS_MEDIA_TOOLBAR_CONTEXT_SHOW_OTHER_SESSIONS);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (!browser_->profile()->IsOffTheRecord() &&
      browser_->profile()->GetPrefs()->GetBoolean(
          prefs::kUserFeedbackAllowed)) {
    menu_model->AddItemWithStringId(
        IDC_MEDIA_TOOLBAR_CONTEXT_REPORT_CAST_ISSUE,
        IDS_MEDIA_TOOLBAR_CONTEXT_REPORT_CAST_ISSUE);
  }
#endif
  return menu_model;
}

bool MediaToolbarButtonContextualMenu::IsCommandIdChecked(
    int command_id) const {
  PrefService* pref_service = browser_->profile()->GetPrefs();
  switch (command_id) {
    case IDC_MEDIA_TOOLBAR_CONTEXT_SHOW_OTHER_SESSIONS:
      return pref_service->GetBoolean(
          media_router::prefs::
              kMediaRouterShowCastSessionsStartedByOtherDevices);
    default:
      return false;
  }
}

void MediaToolbarButtonContextualMenu::ExecuteCommand(int command_id,
                                                      int event_flags) {
  switch (command_id) {
    case IDC_MEDIA_TOOLBAR_CONTEXT_SHOW_OTHER_SESSIONS:
      ToggleShowOtherSessions();
      break;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case IDC_MEDIA_TOOLBAR_CONTEXT_REPORT_CAST_ISSUE:
      ReportIssue();
      break;
#endif
    default:
      NOTREACHED();
  }
}

void MediaToolbarButtonContextualMenu::ToggleShowOtherSessions() {
  PrefService* pref_service = browser_->profile()->GetPrefs();
  pref_service->SetBoolean(
      media_router::prefs::kMediaRouterShowCastSessionsStartedByOtherDevices,
      !pref_service->GetBoolean(
          media_router::prefs::
              kMediaRouterShowCastSessionsStartedByOtherDevices));
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
void MediaToolbarButtonContextualMenu::ReportIssue() {
  ShowSingletonTab(
      browser_,
      GURL(base::StrCat({"chrome://", chrome::kChromeUICastFeedbackHost})));
}
#endif
