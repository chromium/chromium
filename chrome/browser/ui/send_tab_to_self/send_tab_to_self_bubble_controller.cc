// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble_controller.h"

#include <vector>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_desktop_util.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/send_tab_to_self/pref_names.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace send_tab_to_self {

SendTabToSelfBubbleController::~SendTabToSelfBubbleController() {
  if (send_tab_to_self_bubble_view_) {
    send_tab_to_self_bubble_view_->Hide();
  }
}

// Static:
SendTabToSelfBubbleController*
SendTabToSelfBubbleController::CreateOrGetFromWebContents(
    content::WebContents* web_contents) {
  SendTabToSelfBubbleController::CreateForWebContents(web_contents);
  SendTabToSelfBubbleController* controller =
      SendTabToSelfBubbleController::FromWebContents(web_contents);
  return controller;
}

void SendTabToSelfBubbleController::HideBubble() {
  if (send_tab_to_self_bubble_view_) {
    send_tab_to_self_bubble_view_->Hide();
    send_tab_to_self_bubble_view_ = nullptr;
  }
}

void SendTabToSelfBubbleController::ShowBubble() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  send_tab_to_self_bubble_view_ =
      browser->window()->ShowSendTabToSelfBubble(web_contents_, this, true);
}

SendTabToSelfBubbleView*
SendTabToSelfBubbleController::send_tab_to_self_bubble_view() const {
  return send_tab_to_self_bubble_view_;
}

std::u16string SendTabToSelfBubbleController::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_CONTEXT_MENU_SEND_TAB_TO_SELF);
}

std::vector<TargetDeviceInfo> SendTabToSelfBubbleController::GetValidDevices()
    const {
  SendTabToSelfSyncService* const service =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile());
  SendTabToSelfModel* const model =
      service ? service->GetSendTabToSelfModel() : nullptr;
  return model ? model->GetTargetDeviceInfoSortedList()
               : std::vector<TargetDeviceInfo>();
}

Profile* SendTabToSelfBubbleController::GetProfile() const {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

void SendTabToSelfBubbleController::OnDeviceSelected(
    const std::string& target_device_name,
    const std::string& target_device_guid) {
  RecordSendTabToSelfClickResult(kOmniboxIcon,
                                 SendTabToSelfClickResult::kClickItem);
  CreateNewEntry(web_contents_, target_device_name, target_device_guid, GURL());
}

void SendTabToSelfBubbleController::OnBubbleClosed() {
  send_tab_to_self_bubble_view_ = nullptr;
}

void SendTabToSelfBubbleController::ShowConfirmationMessage() {
  show_message_ = true;
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  browser->window()->UpdatePageActionIcon(PageActionIconType::kSendTabToSelf);
}

bool SendTabToSelfBubbleController::InitialSendAnimationShown() const {
  return GetProfile()->GetPrefs()->GetBoolean(
      prefs::kInitialSendAnimationShown);
}

void SendTabToSelfBubbleController::SetInitialSendAnimationShown(bool shown) {
  GetProfile()->GetPrefs()->SetBoolean(prefs::kInitialSendAnimationShown,
                                       shown);
}

// Static:
void SendTabToSelfBubbleController::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  user_prefs->RegisterBooleanPref(prefs::kInitialSendAnimationShown, false);
}

SendTabToSelfBubbleController::SendTabToSelfBubbleController() = default;

SendTabToSelfBubbleController::SendTabToSelfBubbleController(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
  DCHECK(web_contents);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SendTabToSelfBubbleController)

}  // namespace send_tab_to_self
