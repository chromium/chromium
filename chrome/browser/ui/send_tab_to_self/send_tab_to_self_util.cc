// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_util.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_scroll_observer.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_activation_tracker.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/common/url_constants.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/models/image_model.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"
#include "url/origin.h"

namespace send_tab_to_self {

namespace {

// Opens the given `entry` in a new tab with the given `disposition` for the
// given `profile`.
// Returns a weak pointer to the opened WebContents.
base::WeakPtr<content::WebContents> OpenEntryInNewTabWithDisposition(
    Profile* profile,
    const SendTabToSelfEntry& entry,
    WindowOpenDisposition disposition) {
  RecordHasScrollPositionOnOpened(
      !entry.GetPageContext().scroll_position.IsEmpty());

  NavigateParams params(profile, entry.GetURL(), ui::PAGE_TRANSITION_LINK);
  params.disposition = disposition;
  params.window_action = NavigateParams::WindowAction::kShowWindow;

  std::optional<std::string> scroll_to_text_fragment =
      GetScrollPositionAsTextFragment(&entry);
  if (scroll_to_text_fragment) {
    params.internal_scroll_to_text_fragment = *scroll_to_text_fragment;
  }

  base::WeakPtr<content::NavigationHandle> handle = Navigate(&params);

  if (params.navigated_or_inserted_contents) {
    SendTabToSelfScrollObserver::CreateForWebContents(
        params.navigated_or_inserted_contents,
        /*restoration_attempted=*/scroll_to_text_fragment.has_value());
  }

  if (handle) {
    FillWebContents(params.navigated_or_inserted_contents,
                    url::Origin::Create(entry.GetURL()),
                    entry.GetPageContext());
  }

  SendTabToSelfSyncServiceFactory::GetForProfile(profile)
      ->GetSendTabToSelfModel()
      ->MarkEntryOpened(entry.GetGUID());

  return params.navigated_or_inserted_contents
             ? params.navigated_or_inserted_contents->GetWeakPtr()
             : nullptr;
}

void ShowTabSentToastWithDevice(content::WebContents* web_contents,
                                ToastId toast_id,
                                std::string_view device_name,
                                syncer::DeviceInfo::FormFactor form_factor) {
  ToastController* toast_controller =
      ToastController::MaybeGetForWebContents(web_contents);
  if (!toast_controller) {
    return;
  }

  ToastParams params(toast_id);
  // TODO(crbug.com/492072882): The string "Sent to Chrome on your [device]"
  // only really makes sense if `device_name` is a device type/class (e.g.
  // "iPhone" or "Pixel"), not a custom name or hostname. Check with UX
  // whether to use manufacturer + form factor to construct the device name
  // where available, and how to handle edge cases like custom-made PCs.
  params.body_string_replacement_params = {base::UTF8ToUTF16(device_name)};
  params.image_override = ui::ImageModel::FromVectorIcon(
      GetDeviceIcon(form_factor), ui::kColorToastForeground, 20);
  toast_controller->MaybeShowToast(std::move(params));
}

void ShowTabSentFailure(content::WebContents* web_contents,
                        SendTabToSelfResult result) {
  CHECK(web_contents);
  ToastController* toast_controller =
      ToastController::MaybeGetForWebContents(web_contents);
  if (!toast_controller) {
    return;
  }

  ToastId toast_id = ToastId::kSendTabToSelfFailure;
  if (result == SendTabToSelfResult::kFailureNoInternetConnection ||
      result == SendTabToSelfResult::kFailureCommitTimeout) {
    toast_id = ToastId::kSendTabToSelfNoInternetConnection;
  }
  toast_controller->MaybeShowToast(ToastParams(toast_id));
}

}  // namespace

const gfx::VectorIcon& GetDeviceIcon(
    syncer::DeviceInfo::FormFactor form_factor) {
  switch (form_factor) {
    case syncer::DeviceInfo::FormFactor::kPhone:
      return features::IsRoundedIconsEnabled() ? kMobileIcon
                                               : kHardwareSmartphoneOldIcon;
    case syncer::DeviceInfo::FormFactor::kTablet:
      return features::IsRoundedIconsEnabled() ? kTabletFilledIcon
                                               : kTabletOldIcon;
    default:
      return features::IsRoundedIconsEnabled() ? kComputerCustomIcon
                                               : kHardwareComputerOldIcon;
  }
}

void MarkEntryMatchingGuidActivated(Profile* profile,
                                    const std::string& guid,
                                    ShareActivatedEntryPoint entry_point) {
  auto* sync_service = SendTabToSelfSyncServiceFactory::GetForProfile(profile);
  SendTabToSelfModel* model =
      sync_service ? sync_service->GetSendTabToSelfModel() : nullptr;
  if (model) {
    model->MarkEntryActivated(guid, entry_point);
  }
}

base::WeakPtr<content::WebContents> OpenEntryInNewForegroundTab(
    Profile* profile,
    const SendTabToSelfEntry& entry,
    ShareActivatedEntryPoint entry_point) {
  base::WeakPtr<content::WebContents> web_contents =
      OpenEntryInNewTabWithDisposition(
          profile, entry, WindowOpenDisposition::kNewForegroundTab);
  if (web_contents) {
    MarkEntryMatchingGuidActivated(profile, entry.GetGUID(), entry_point);
  }
  return web_contents;
}

base::WeakPtr<content::WebContents> OpenEntryInNewBackgroundTab(
    Profile* profile,
    const SendTabToSelfEntry& entry) {
  base::WeakPtr<content::WebContents> web_contents =
      OpenEntryInNewTabWithDisposition(
          profile, entry, WindowOpenDisposition::kNewBackgroundTab);
  if (web_contents) {
    SendTabToSelfActivationTracker::CreateForWebContents(web_contents.get(),
                                                         entry.GetGUID());
  }
  return web_contents;
}

void OnSendTabToDeviceComplete(base::WeakPtr<content::WebContents> web_contents,
                               const std::string& device_name,
                               syncer::DeviceInfo::FormFactor form_factor,
                               SendTabToSelfResult result) {
  if (!web_contents) {
    return;
  }

  switch (result) {
    case SendTabToSelfResult::kSuccess:
      ShowTabSentToastWithDevice(web_contents.get(),
                                 ToastId::kSendTabToSelfSuccess, device_name,
                                 form_factor);
      break;
    case SendTabToSelfResult::kSuccessThrottled:
      ShowTabSentToastWithDevice(web_contents.get(),
                                 ToastId::kSendTabToSelfSuccessThrottled,
                                 device_name, form_factor);
      break;
    case SendTabToSelfResult::kFailureInvalidUrl:
    case SendTabToSelfResult::kFailureNotTrackingMetadata:
    case SendTabToSelfResult::kFailureCommitAttemptFailed:
    case SendTabToSelfResult::kFailureCommitAttemptError:
    case SendTabToSelfResult::kFailureSyncDisabled:
    case SendTabToSelfResult::kFailureEntryRemoved:
    case SendTabToSelfResult::kFailureCommitTimeout:
    case SendTabToSelfResult::kFailureNoInternetConnection:
      ShowTabSentFailure(web_contents.get(), result);
      break;
  }
}

void OpenManageDevicesPage(Profile* profile,
                           WindowOpenDisposition disposition) {
  CHECK(profile);
  NavigateParams params(profile, GURL(chrome::kGoogleAccountDeviceActivityURL),
                        ui::PAGE_TRANSITION_LINK);
  params.disposition = disposition;
  Navigate(&params);
}

}  // namespace send_tab_to_self
