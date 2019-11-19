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
  RecordSendTabToSelfClickResult(kOmniboxIcon,
                                 SendTabToSelfClickResult::kShowDeviceList);
  RecordSendTabToSelfDeviceCount(kOmniboxIcon, GetValidDevices().size());
}

SendTabToSelfBubbleView*
SendTabToSelfBubbleController::send_tab_to_self_bubble_view() const {
  return send_tab_to_self_bubble_view_;
}

base::string16 SendTabToSelfBubbleController::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_CONTEXT_MENU_SEND_TAB_TO_SELF);
}

const std::vector<TargetDeviceInfo>&
SendTabToSelfBubbleController::GetValidDevices() const {
  return valid_devices_;
}

Profile* SendTabToSelfBubbleController::GetProfile() const {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

void SendTabToSelfBubbleController::OnDeviceSelected(
    const std::string& target_device_name,
    const std::string& target_device_guid) {
  RecordSendTabToSelfClickResult(kOmniboxIcon,
                                 SendTabToSelfClickResult::kClickItem);
  CreateNewEntry(web_contents_, target_device_name, target_device_guid, GURL(),
                 false);
  show_message_ = true;
  UpdateIcon();
}

void SendTabToSelfBubbleController::OnBubbleClosed() {
  send_tab_to_self_bubble_view_ = nullptr;
}

SendTabToSelfBubbleController::SendTabToSelfBubbleController() = default;

SendTabToSelfBubbleController::SendTabToSelfBubbleController(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
  DCHECK(web_contents);
  FetchDeviceInfo();
}

void SendTabToSelfBubbleController::UpdateIcon() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  browser->window()->UpdatePageActionIcon(PageActionIconType::kSendTabToSelf);
}

void SendTabToSelfBubbleController::FetchDeviceInfo() {
  valid_devices_.clear();
  SendTabToSelfSyncService* service =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile());
  if (!service) {
    return;
  }
  SendTabToSelfModel* model = service->GetSendTabToSelfModel();
  if (!model) {
    return;
  }
  valid_devices_ = model->GetTargetDeviceInfoSortedList();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SendTabToSelfBubbleController)

}  // namespace send_tab_to_self
