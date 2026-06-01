// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_controller.h"

#include <algorithm>
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
#include "chrome/browser/sync/sync_service_factory.h"
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
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/common/url_constants.h"
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
#include "components/sync/service/sync_service.h"
#include "components/sync_device_info/device_info.h"
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

namespace {

// TODO(crbug.com/492072882): Inefficiently fetches the whole sorted list just
// to find one device by GUID. There should exist
// SendTabToSelfModel::GetTargetDeviceInfo(guid)
syncer::DeviceInfo::FormFactor GetFormFactorForDevice(
    SendTabToSelfModel* model,
    const std::string& target_device_guid) {
  if (!model) {
    return syncer::DeviceInfo::FormFactor::kUnknown;
  }
  const std::vector<TargetDeviceInfo> devices =
      model->GetTargetDeviceInfoSortedList();
  std::vector<TargetDeviceInfo>::const_iterator it = std::ranges::find(
      devices, target_device_guid, &TargetDeviceInfo::cache_guid);
  return it != devices.end() ? it->form_factor
                             : syncer::DeviceInfo::FormFactor::kUnknown;
}

}  // namespace

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

  std::optional<send_tab_to_self::EntryPointDisplayReason> reason =
      GetEntryPointDisplayReason();

  if (!reason) {
    // If the user has just signed in, the model might not be ready yet.
    // Defer the bubble display until the model is fully loaded and ready.
    if (ShouldStartWaitingForModel()) {
      StartWaitingForModel();
    }
    return;
  }

  // If we were waiting for the model but it is now ready, clear the waiting
  // state.
  if (model_observation_.IsObserving()) {
    model_observation_.Reset();
  }

  ShowBubbleImpl(*reason);
}

void SendTabToSelfBubbleController::ShowBubbleImpl(
    EntryPointDisplayReason reason) {
  BrowserWindowInterface* browser =
      GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(
          &GetWebContents());

  base::WeakPtr<BrowserWindowInterface> browser_weak_ptr;
  views::BubbleAnchor anchor;
  if (browser) {
    browser_weak_ptr = browser->GetWeakPtr();
    if (PinnedToolbarActions* pinned_toolbar_actions =
            browser->GetFeatures().pinned_toolbar_actions()) {
      // Show the toolbar action button.
      pinned_toolbar_actions->ShowActionEphemerallyInToolbar(
          kActionSendTabToSelf, true);
      pinned_toolbar_actions->GetBubbleAnchorAsync(
          kActionSendTabToSelf,
          base::BindOnce(&SendTabToSelfBubbleController::ShowBubbleWithAnchor,
                         weak_ptr_factory_.GetWeakPtr(), reason,
                         browser_weak_ptr));
      return;
    }
    anchor = ToolbarButtonProvider::From(browser)->GetBubbleAnchor(
        kActionSendTabToSelf);
  }

  ShowBubbleWithAnchor(reason, browser_weak_ptr, std::move(anchor));
}

void SendTabToSelfBubbleController::ShowBubbleWithAnchor(
    EntryPointDisplayReason reason,
    base::WeakPtr<BrowserWindowInterface> browser,
    BubbleAnchorResult anchor) {
  if (!anchor.has_value()) {
    if (!browser) {
      return;
    }
    // PinnedToolbarActions failed to find an anchor. Try ToolbarButtonProvider
    // as it has fallback anchor logic.
    auto new_anchor = ToolbarButtonProvider::From(browser.get())
                          ->GetBubbleAnchor(kActionSendTabToSelf);
    if (new_anchor.IsNull()) {
      return;
    }
    anchor = new_anchor;
  }

  std::unique_ptr<SendTabToSelfBubbleView> bubble_view;
  switch (reason) {
    case send_tab_to_self::EntryPointDisplayReason::kOfferFeature:
      bubble_view = std::make_unique<SendTabToSelfDevicePickerBubbleView>(
          std::move(anchor.value()), &GetWebContents());
      break;
    case send_tab_to_self::EntryPointDisplayReason::kOfferSignIn: {
      bubble_view = std::make_unique<SendTabToSelfSignInPromoBubbleView>(
          std::move(anchor.value()), &GetWebContents(),
          /*is_account_aware=*/!GetSharingAccountInfo().IsEmpty());
      break;
    }
    case send_tab_to_self::EntryPointDisplayReason::kInformNoTargetDevice:
      bubble_view = std::make_unique<SendTabToSelfNoTargetDeviceBubbleView>(
          std::move(anchor.value()), &GetWebContents());
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

send_tab_to_self::SendTabToSelfModel*
SendTabToSelfBubbleController::GetModel() {
  send_tab_to_self::SendTabToSelfSyncService* service =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile());
  return service ? service->GetSendTabToSelfModel() : nullptr;
}

std::optional<send_tab_to_self::EntryPointDisplayReason>
SendTabToSelfBubbleController::GetEntryPointDisplayReason() {
  return send_tab_to_self::GetEntryPointDisplayReason(&GetWebContents());
}

void SendTabToSelfBubbleController::OnDeviceSelected(
    const std::string& target_device_guid,
    std::string_view device_name) {
  UserEducationService::MaybeNotifyNewBadgeFeatureUsed(
      GetProfile(), send_tab_to_self::kSendTabToSelfEnhancedDesktopUI);

  // TODO(crbug.com/40817150): This duplicates the ShouldOfferFeature() check,
  // instead the 2 codepaths should share code.
  SendTabToSelfPageHandler* handler =
      SendTabToSelfPageHandler::GetOrCreateForWebContents(&GetWebContents());

  syncer::DeviceInfo::FormFactor form_factor =
      GetFormFactorForDevice(GetModel(), target_device_guid);

  const GURL url = GetWebContents().GetLastCommittedURL();
  handler->SendTabToDevice(
      target_device_guid, url, base::UTF16ToUTF8(GetWebContents().GetTitle()),
      base::BindOnce(
          &SendTabToSelfBubbleController::HandleSendTabToDeviceResult,
          weak_ptr_factory_.GetWeakPtr(), url, std::string(device_name),
          form_factor));
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
    syncer::DeviceInfo::FormFactor form_factor,
    SendTabToSelfResult result) {
  switch (result) {
    case SendTabToSelfResult::kSuccess:
      if (base::FeatureList::IsEnabled(kSendTabToSelfPostSendToast)) {
        ShowTabSentSuccessToast(&GetWebContents(), device_name, form_factor);
      }
      break;
    case SendTabToSelfResult::kSuccessThrottled:
      if (base::FeatureList::IsEnabled(kSendTabToSelfPostSendToast)) {
        ShowTabSentThrottledToast(&GetWebContents(), device_name, form_factor);
      }
      break;
    case SendTabToSelfResult::kFailureInvalidUrl:
    case SendTabToSelfResult::kFailureNotTrackingMetadata:
    case SendTabToSelfResult::kFailureCommitAttemptFailed:
    case SendTabToSelfResult::kFailureCommitAttemptError:
    case SendTabToSelfResult::kFailureSyncDisabled:
    case SendTabToSelfResult::kFailureEntryRemoved:
    case SendTabToSelfResult::kFailureCommitTimeout:
    case SendTabToSelfResult::kFailureNoInternetConnection:
      ShowTabSentFailure(&GetWebContents(), result, url);
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

void SendTabToSelfBubbleController::OnEntriesAddedRemotely(
    const std::vector<const SendTabToSelfEntry*>& new_entries) {}

void SendTabToSelfBubbleController::OnEntriesRemovedRemotely(
    const std::vector<std::string>& guids) {}

void SendTabToSelfBubbleController::OnModelReady() {
  model_observation_.Reset();

  std::optional<send_tab_to_self::EntryPointDisplayReason> reason =
      GetEntryPointDisplayReason();
  // If the user signed out or sync has been disabled during the asynchronous
  // wait, the model will no longer be in a state where we should show the
  // bubble.
  if (!reason.has_value() ||
      reason.value() == EntryPointDisplayReason::kOfferSignIn) {
    return;
  }

  ShowBubbleImpl(*reason);
}

bool SendTabToSelfBubbleController::ShouldStartWaitingForModel() {
  send_tab_to_self::SendTabToSelfModel* model = GetModel();
  return model && !model->IsReady() && !model_observation_.IsObserving();
}

void SendTabToSelfBubbleController::StartWaitingForModel() {
  send_tab_to_self::SendTabToSelfModel* model = GetModel();
  if (model && !model_observation_.IsObserving()) {
    model_observation_.Observe(model);
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
