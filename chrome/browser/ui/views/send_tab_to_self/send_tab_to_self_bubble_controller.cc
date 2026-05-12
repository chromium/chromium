// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_controller.h"

#include <string_view>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_page_handler.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_controller.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_view.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_device_picker_bubble_view.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_promo_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/send_tab_to_self/pref_names.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/events/event.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace send_tab_to_self {

SendTabToSelfBubbleController::~SendTabToSelfBubbleController() {
  HideBubble();
}


void SendTabToSelfBubbleController::HideBubble() {
  if (send_tab_to_self_bubble_view_) {
    send_tab_to_self_bubble_view_->Hide();
  }
}

void SendTabToSelfBubbleController::ShowBubble(bool show_back_button) {
  // Avoid re-creation if a bubble is already being shown for this controller.
  if (send_tab_to_self_bubble_view_) {
    return;
  }

  show_back_button_ = show_back_button;
  BrowserWindowInterface* browser =
      GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(
          &GetWebContents());
  std::optional<send_tab_to_self::EntryPointDisplayReason> reason =
      GetEntryPointDisplayReason();
  CHECK(reason);

  views::BubbleAnchor anchor;
  if (browser) {
    if (PinnedToolbarActions* pinned_toolbar_actions =
            browser->GetFeatures().pinned_toolbar_actions()) {
      // Show the toolbar action button.
      pinned_toolbar_actions->ShowActionEphemerallyInToolbar(
          kActionSendTabToSelf, true);
      anchor = pinned_toolbar_actions->GetBubbleAnchor(kActionSendTabToSelf);
    } else {
      anchor = ToolbarButtonProvider::From(browser)->GetBubbleAnchor(
          kActionSendTabToSelf);
    }
  }

  std::unique_ptr<SendTabToSelfBubbleView> bubble_view;
  switch (*reason) {
    case send_tab_to_self::EntryPointDisplayReason::kOfferFeature:
      bubble_view = std::make_unique<SendTabToSelfDevicePickerBubbleView>(
          std::move(anchor), &GetWebContents());
      break;
    case send_tab_to_self::EntryPointDisplayReason::kOfferSignIn: {
      const SendTabToSelfPromoBubbleView::PromoType promo_type =
          GetSharingAccountInfo().IsEmpty()
              ? SendTabToSelfPromoBubbleView::PromoType::kSignInPromo
              : SendTabToSelfPromoBubbleView::PromoType::
                    kAccountAwareSignInPromo;
      bubble_view = std::make_unique<SendTabToSelfPromoBubbleView>(
          std::move(anchor), &GetWebContents(), promo_type);
      break;
    }
    case send_tab_to_self::EntryPointDisplayReason::kInformNoTargetDevice:
      bubble_view = std::make_unique<SendTabToSelfPromoBubbleView>(
          std::move(anchor), &GetWebContents(),
          SendTabToSelfPromoBubbleView::PromoType::kNoTargetDevice);
      break;
  }
  send_tab_to_self_bubble_view_ = bubble_view.get();
  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(std::move(bubble_view));
  widget_observation_.Observe(widget);
  send_tab_to_self_bubble_view_->SetHighlightedElement(
      kPinnedToolbarActionSendTabToSelfElementId);
  // This is always triggered due to a user gesture, c.f. method
  // documentation.
  send_tab_to_self_bubble_view_->ShowForReason(
      LocationBarBubbleDelegateView::USER_GESTURE);

  if (browser) {
    send_tab_to_self_action_item_ = actions::ActionManager::Get().FindAction(
        kActionSendTabToSelf, browser->GetActions()->root_action_item());
    // The toolbar might not have this action button.
    // See SendTabToSelfToolbarIconController::CanShowOnBrowser().
    if (send_tab_to_self_action_item_) {
      send_tab_to_self_action_item_->SetIsShowingBubble(true);
    }
  }
}

bool SendTabToSelfBubbleController::IsBubbleShown() const {
  return send_tab_to_self_bubble_view_;
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

std::optional<send_tab_to_self::EntryPointDisplayReason>
SendTabToSelfBubbleController::GetEntryPointDisplayReason() {
  return send_tab_to_self::GetEntryPointDisplayReason(&GetWebContents());
}

void SendTabToSelfBubbleController::OnDeviceSelected(
    const std::string& target_device_guid,
    std::string_view device_name) {
  // TODO(crbug.com/40817150): This duplicates the ShouldOfferFeature() check,
  // instead the 2 codepaths should share code.
  SendTabToSelfPageHandler* handler =
      SendTabToSelfPageHandler::GetOrCreateForWebContents(&GetWebContents());

  const GURL url = GetWebContents().GetLastCommittedURL();
  handler->SendTabToDevice(
      target_device_guid, url, base::UTF16ToUTF8(GetWebContents().GetTitle()),
      base::BindOnce(
          &SendTabToSelfBubbleController::HandleSendTabToDeviceResult,
          weak_ptr_factory_.GetWeakPtr(), url, std::string(device_name)));
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

void SendTabToSelfBubbleController::OnWidgetDestroying(views::Widget* widget) {
  widget_observation_.Reset();
  send_tab_to_self_bubble_view_ = nullptr;
  BrowserWindowInterface* browser =
      GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(
          &GetWebContents());
  if (browser && browser->GetFeatures().pinned_toolbar_actions()) {
    // Hide the toolbar action button.
    browser->GetFeatures()
        .pinned_toolbar_actions()
        ->ShowActionEphemerallyInToolbar(kActionSendTabToSelf, false);
  }
  if (send_tab_to_self_action_item_) {
    send_tab_to_self_action_item_->SetIsShowingBubble(false);
  }
}

void SendTabToSelfBubbleController::OnBackButtonPressed() {
  sharing_hub::SharingHubBubbleController* controller =
      sharing_hub::SharingHubBubbleController::CreateOrGetFromWebContents(
          &GetWebContents());
  controller->ShowBubble(share::ShareAttempt(&GetWebContents()));
}

void SendTabToSelfBubbleController::HandleSendTabToDeviceResult(
    const GURL& url,
    std::string_view device_name,
    SendTabToSelfResult result) {
  switch (result) {
    case SendTabToSelfResult::kSuccess:
    case SendTabToSelfResult::kSuccessThrottled:
      if (base::FeatureList::IsEnabled(kSendTabToSelfPostSendToast)) {
        ShowTabSentSuccessToast(&GetWebContents(), device_name);
      }
      break;
    case SendTabToSelfResult::kFailureInvalidUrl:
    case SendTabToSelfResult::kFailureNotTrackingMetadata:
    case SendTabToSelfResult::kFailureCommitAttemptFailed:
    case SendTabToSelfResult::kFailureCommitAttemptError:
    case SendTabToSelfResult::kFailureSyncDisabled:
    case SendTabToSelfResult::kFailureEntryRemoved:
    case SendTabToSelfResult::kFailureCommitTimeout:
      ShowTabSentFailure(&GetWebContents(), url);
      break;
  }
}

bool SendTabToSelfBubbleController::InitialSendAnimationShown() {
  return GetProfile()->GetPrefs()->GetBoolean(
      prefs::kInitialSendAnimationShown);
}

void SendTabToSelfBubbleController::SetInitialSendAnimationShown(bool shown) {
  GetProfile()->GetPrefs()->SetBoolean(prefs::kInitialSendAnimationShown,
                                       shown);
}

void SendTabToSelfBubbleController::SetSelectorGenerationTimeoutForTesting(
    base::TimeDelta timeout) {
  SendTabToSelfPageHandler::GetOrCreateForWebContents(&GetWebContents())
      ->SetSelectorGenerationTimeoutForTesting(timeout);
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
