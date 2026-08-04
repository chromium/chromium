// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_contextual_menu.h"

#include <memory>

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service_factory.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/global_media_controls/public/media_item_manager.h"
#include "components/media_router/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace {
global_media_controls::MediaItemManager* GetItemManagerFromProfile(
    Profile* profile) {
  auto* service = MediaNotificationServiceFactory::GetForProfile(profile);
  return service ? service->media_item_manager() : nullptr;
}
}  // namespace

MediaToolbarButtonContextualMenu::MediaToolbarButtonContextualMenu(
    Profile* profile)
    : profile_(profile) {}

MediaToolbarButtonContextualMenu::~MediaToolbarButtonContextualMenu() = default;

std::unique_ptr<ui::SimpleMenuModel>
MediaToolbarButtonContextualMenu::CreateMenuModel() {
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(this);
  menu_model->AddCheckItemWithStringId(
      IDC_MEDIA_TOOLBAR_CONTEXT_SHOW_OTHER_SESSIONS,
      IDS_MEDIA_TOOLBAR_CONTEXT_SHOW_OTHER_SESSIONS);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (chrome::CanShowFeedback(profile_)) {
    menu_model->AddItemWithStringId(
        IDC_MEDIA_TOOLBAR_CONTEXT_REPORT_CAST_ISSUE,
        IDS_MEDIA_TOOLBAR_CONTEXT_REPORT_CAST_ISSUE);
  }
#endif
  return menu_model;
}

bool MediaToolbarButtonContextualMenu::IsCommandIdChecked(
    int command_id) const {
  PrefService* pref_service = profile_->GetPrefs();
  switch (command_id) {
    case IDC_MEDIA_TOOLBAR_CONTEXT_SHOW_OTHER_SESSIONS:
      return pref_service->GetBoolean(
          media_router::prefs::
              kMediaRouterShowCastSessionsStartedByOtherDevices);
    default:
      return false;
  }
}

bool MediaToolbarButtonContextualMenu::IsCommandIdEnabled(
    int command_id) const {
  PrefService* pref_service = profile_->GetPrefs();
  switch (command_id) {
    case IDC_MEDIA_TOOLBAR_CONTEXT_SHOW_OTHER_SESSIONS:
      // The pref may be managed by an enterprise policy and not modifiable by
      // the user, in which case we disable the menu item.
      return pref_service->IsUserModifiablePreference(
          media_router::prefs::
              kMediaRouterShowCastSessionsStartedByOtherDevices);
    default:
      return true;
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

void MediaToolbarButtonContextualMenu::MenuClosed(ui::SimpleMenuModel* source) {
  if (!profile_) {
    return;
  }
  auto* item_manager = GetItemManagerFromProfile(profile_);
  if (item_manager) {
    item_manager->OnItemsChanged();
  }
}

void MediaToolbarButtonContextualMenu::ToggleShowOtherSessions() {
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(
      media_router::prefs::kMediaRouterShowCastSessionsStartedByOtherDevices,
      !pref_service->GetBoolean(
          media_router::prefs::
              kMediaRouterShowCastSessionsStartedByOtherDevices));
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
void MediaToolbarButtonContextualMenu::ReportIssue() {
  ShowSingletonTab(
      profile_,
      GURL(base::StrCat({"chrome://", chrome::kChromeUICastFeedbackHost})));
}
#endif
