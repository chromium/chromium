// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_controller.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/desktop_notification_handler.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_controller.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_view.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/send_tab_to_self/pref_names.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/events/event.h"

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

void SendTabToSelfBubbleController::ShowBubble(bool show_back_button) {
  show_back_button_ = show_back_button;
  bubble_shown_ = true;
  Browser* browser = chrome::FindBrowserWithTab(&GetWebContents());
  absl::optional<send_tab_to_self::EntryPointDisplayReason> reason =
      send_tab_to_self::GetEntryPointDisplayReason(&GetWebContents());
  DCHECK(reason);
  switch (*reason) {
    case send_tab_to_self::EntryPointDisplayReason::kOfferFeature:
      send_tab_to_self::RecordSendingEvent(ShareEntryPoint::kOmniboxIcon,
                                           SendingEvent::kShowDeviceList);
      send_tab_to_self_bubble_view_ =
          browser->window()->ShowSendTabToSelfDevicePickerBubble(
              &GetWebContents());
      break;
    case send_tab_to_self::EntryPointDisplayReason::kOfferSignIn:
      send_tab_to_self::RecordSendingEvent(ShareEntryPoint::kOmniboxIcon,
                                           SendingEvent::kShowSigninPromo);
      send_tab_to_self_bubble_view_ =
          browser->window()->ShowSendTabToSelfPromoBubble(
              &GetWebContents(), /*show_signin_button=*/true);
      break;
    case send_tab_to_self::EntryPointDisplayReason::kInformNoTargetDevice:
      send_tab_to_self::RecordSendingEvent(
          ShareEntryPoint::kOmniboxIcon,
          SendingEvent::kShowNoTargetDeviceMessage);
      send_tab_to_self_bubble_view_ =
          browser->window()->ShowSendTabToSelfPromoBubble(
              &GetWebContents(), /*show_signin_button=*/false);
      break;
  }

  if (sharing_hub::SharingHubOmniboxEnabled(
          GetWebContents().GetBrowserContext())) {
    UpdateIcon();
  }
}

SendTabToSelfBubbleView*
SendTabToSelfBubbleController::send_tab_to_self_bubble_view() const {
  return send_tab_to_self_bubble_view_;
}

std::vector<TargetDeviceInfo> SendTabToSelfBubbleController::GetValidDevices() {
  SendTabToSelfSyncService* const service =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile());
  SendTabToSelfModel* const model =
      service ? service->GetSendTabToSelfModel() : nullptr;
  return model ? model->GetTargetDeviceInfoSortedList()
               : std::vector<TargetDeviceInfo>();
}

AccountInfo SendTabToSelfBubbleController::GetSharingAccountInfo() {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(GetProfile());
  return identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
}

Profile* SendTabToSelfBubbleController::GetProfile() {
  return Profile::FromBrowserContext(GetWebContents().GetBrowserContext());
}

void SendTabToSelfBubbleController::OnDeviceSelected(
    const std::string& target_device_guid) {
  // TODO(crbug.com/1288843): This is being recorded for entry points other
  // than the omnibox. Make the entry point a ShowBubble() argument.
  send_tab_to_self::RecordSendingEvent(ShareEntryPoint::kOmniboxIcon,
                                       SendingEvent::kClickItem);

  SendTabToSelfModel* model =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile())
          ->GetSendTabToSelfModel();
  // TODO(crbug.com/1288843): This duplicates the ShouldOfferFeature() check,
  // instead the 2 codepaths should share code.
  const GURL& shared_url = GetWebContents().GetLastCommittedURL();
  if (!model->IsReady()) {
    // TODO(https://crbug.com/1280681): Is this legit? In STTSv2, there may not
    // *be* a DesktopNotificationHandler for profile, and we're violating the
    // lifetime rules of DesktopNotificationHandler here I think.
    DesktopNotificationHandler(GetProfile()).DisplayFailureMessage(shared_url);
    return;
  }

  model->AddEntry(shared_url, base::UTF16ToUTF8(GetWebContents().GetTitle()),
                  target_device_guid);
  // Show confirmation message.
  show_message_ = true;
  UpdateIcon();
}

void SendTabToSelfBubbleController::OnManageDevicesClicked(
    const ui::Event& event) {
  NavigateParams params(GetProfile(),
                        GURL(chrome::kGoogleAccountDeviceActivityURL),
                        ui::PageTransition::PAGE_TRANSITION_LINK);
  // NEW_FOREGROUND_TAB is passed as the default below to avoid exiting the
  // current page, which the user possibly wants to share (maybe they just
  // clicked "Manage devices" by mistake). Still, DispositionFromEventFlags()
  // ensures that any modifier keys are respected, e.g. to open a new window
  // instead.
  params.disposition = ui::DispositionFromEventFlags(
      event.flags(), WindowOpenDisposition::NEW_FOREGROUND_TAB);
  Navigate(&params);
}

void SendTabToSelfBubbleController::OnBubbleClosed() {
  bubble_shown_ = false;
  send_tab_to_self_bubble_view_ = nullptr;
}

void SendTabToSelfBubbleController::OnBackButtonPressed() {
  sharing_hub::SharingHubBubbleController* controller =
      sharing_hub::SharingHubBubbleController::CreateOrGetFromWebContents(
          &GetWebContents());
  controller->ShowBubble(share::ShareAttempt(&GetWebContents()));
}

bool SendTabToSelfBubbleController::InitialSendAnimationShown() {
  return GetProfile()->GetPrefs()->GetBoolean(
      prefs::kInitialSendAnimationShown);
}

void SendTabToSelfBubbleController::SetInitialSendAnimationShown(bool shown) {
  GetProfile()->GetPrefs()->SetBoolean(prefs::kInitialSendAnimationShown,
                                       shown);
}

void SendTabToSelfBubbleController::UpdateIcon() {
  Browser* browser = chrome::FindBrowserWithTab(&GetWebContents());
  // UpdateIcon() can be called during browser teardown.
  if (!browser)
    return;

  if (sharing_hub::SharingHubOmniboxEnabled(
          GetWebContents().GetBrowserContext())) {
    browser->window()->UpdatePageActionIcon(PageActionIconType::kSharingHub);
  } else {
    browser->window()->UpdatePageActionIcon(PageActionIconType::kSendTabToSelf);
  }
}

// Static:
void SendTabToSelfBubbleController::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  user_prefs->RegisterBooleanPref(prefs::kInitialSendAnimationShown, false);
}

SendTabToSelfBubbleController::SendTabToSelfBubbleController(
    content::WebContents* web_contents)
    : content::WebContentsUserData<SendTabToSelfBubbleController>(
          *web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SendTabToSelfBubbleController);

}  // namespace send_tab_to_self
