// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_context_menu_delegate.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_page_handler.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/color/color_id.h"

namespace send_tab_to_self {

namespace {

static_assert(IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE_LAST -
                      IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE1 + 1 ==
                  kMaxDevices,
              "kMaxDevices must match the number of command IDs reserved for "
              "target devices in chrome_command_ids.h");

void OnSendTabToDeviceComplete(base::WeakPtr<content::WebContents> web_contents,
                               std::string_view device_name,
                               syncer::DeviceInfo::FormFactor form_factor,
                               SendTabToSelfResult result) {
  if (!web_contents ||
      !base::FeatureList::IsEnabled(kSendTabToSelfPostSendToast)) {
    return;
  }

  switch (result) {
    case SendTabToSelfResult::kSuccess:
      ShowTabSentSuccessToast(web_contents.get(), device_name, form_factor);
      break;
    case SendTabToSelfResult::kSuccessThrottled:
      ShowTabSentThrottledToast(web_contents.get(), device_name, form_factor);
      break;
    case SendTabToSelfResult::kFailureInvalidUrl:
    case SendTabToSelfResult::kFailureNotTrackingMetadata:
    case SendTabToSelfResult::kFailureCommitAttemptFailed:
    case SendTabToSelfResult::kFailureCommitAttemptError:
    case SendTabToSelfResult::kFailureSyncDisabled:
    case SendTabToSelfResult::kFailureEntryRemoved:
    case SendTabToSelfResult::kFailureCommitTimeout:
    case SendTabToSelfResult::kFailureNoInternetConnection:
      ShowTabSentFailure(web_contents.get(), result, GURL());
      break;
  }
}

// Returns `target_url` if valid; otherwise falls back to the last committed URL
// of `web_contents`. Since `web_contents` is null if unavailable, subsequent
// queries will fail anyway so falling back to `GURL()` is alright.
GURL ResolveTargetUrl(const GURL& target_url,
                      content::WebContents* web_contents) {
  if (target_url.is_valid()) {
    return target_url;
  }
  return web_contents ? web_contents->GetLastCommittedURL() : GURL();
}

// Returns `target_title` if non-empty; otherwise falls back to the active page
// title of `web_contents`.
// TODO(crbug.com/530097533): Investigate improved title handling when the user
// interacts with the right-click flow on a hyperlink (e.g., avoiding parent
// page title fallback when link anchor text is empty).
std::string ResolveTargetTitle(const std::string& target_title,
                               content::WebContents* web_contents) {
  if (!target_title.empty()) {
    return target_title;
  }
  return web_contents ? base::UTF16ToUTF8(web_contents->GetTitle())
                      : std::string();
}

std::vector<base::WeakPtr<content::WebContents>> GetWeakWebContentsList(
    base::span<content::WebContents* const> web_contents_list) {
  std::vector<base::WeakPtr<content::WebContents>> list;
  list.reserve(web_contents_list.size());
  for (content::WebContents* web_contents : web_contents_list) {
    if (web_contents) {
      list.push_back(web_contents->GetWeakPtr());
    }
  }
  return list;
}

// Returns the list of target devices to show in the context menu.
// The returned list is capped at `kMaxDevices`.
std::vector<TargetDeviceInfo> GetDevicesForDisplay(
    content::WebContents* web_contents) {
  CHECK(web_contents);

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  SendTabToSelfSyncService* service =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile);
  if (!service) {
    return {};
  }

  std::vector<TargetDeviceInfo> devices =
      service->GetSendTabToSelfModel()->GetTargetDeviceInfoSortedList();
  // To keep the context menu from growing too large, the list of target
  // devices is capped. The exact limit is defined by `kMaxDevices` and matches
  // the number of statically allocated command IDs in chrome_command_ids.h.
  if (devices.size() > kMaxDevices) {
    devices.erase(devices.begin() + kMaxDevices, devices.end());
  }

  return devices;
}

bool ShouldShowSubmenu(EntryPointDisplayReason reason) {
  switch (reason) {
    case EntryPointDisplayReason::kOfferFeature:
      return true;
    case EntryPointDisplayReason::kOfferSignIn:
    case EntryPointDisplayReason::kOfferReauth:
      return base::FeatureList::IsEnabled(kSendTabToSelfSubmenuSigninPromos);
    case EntryPointDisplayReason::kInformNoTargetDevice:
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      return base::FeatureList::IsEnabled(kSendTabToSelfSubmenuSigninPromos) &&
             base::FeatureList::IsEnabled(kSendTabToSelfNoTargetDeviceQrCode);
#else
      return false;
#endif
  }
}

}  // namespace

// static
std::unique_ptr<SendTabToSelfContextMenuDelegate>
SendTabToSelfContextMenuDelegate::MaybeCreateForTab(
    content::WebContents* primary_web_contents,
    ShareEntryPoint entry_point,
    const GURL& target_url,
    const std::string& target_title) {
  if (!primary_web_contents) {
    return nullptr;
  }

  std::optional<EntryPointDisplayReason> reason = GetEntryPointDisplayReason(
      primary_web_contents, ResolveTargetUrl(target_url, primary_web_contents));
  if (!reason.has_value() || !ShouldShowSubmenu(*reason)) {
    return nullptr;
  }

  std::vector<TargetDeviceInfo> devices;
  if (*reason == EntryPointDisplayReason::kOfferFeature) {
    devices = GetDevicesForDisplay(primary_web_contents);
    if (devices.empty()) {
      return nullptr;
    }
  }

  return base::WrapUnique(new SendTabToSelfContextMenuDelegate(
      primary_web_contents, *reason, std::move(devices), entry_point,
      target_url, target_title));
}

// static
std::unique_ptr<SendTabToSelfContextMenuDelegate>
SendTabToSelfContextMenuDelegate::MaybeCreateForMultipleTabs(
    content::WebContents* primary_web_contents,
    base::span<content::WebContents* const> web_contents_list,
    ShareEntryPoint entry_point) {
  std::unique_ptr<SendTabToSelfContextMenuDelegate> delegate =
      MaybeCreateForTab(primary_web_contents, entry_point);
  if (delegate) {
    delegate->web_contents_list_ = GetWeakWebContentsList(web_contents_list);
  }
  return delegate;
}

SendTabToSelfContextMenuDelegate::SendTabToSelfContextMenuDelegate(
    content::WebContents* primary_web_contents,
    EntryPointDisplayReason display_reason,
    std::vector<TargetDeviceInfo> devices,
    ShareEntryPoint entry_point,
    const GURL& target_url,
    const std::string& target_title)
    : primary_web_contents_(primary_web_contents->GetWeakPtr()),
      web_contents_list_(
          GetWeakWebContentsList(base::span_from_ref(primary_web_contents))),
      display_reason_(display_reason),
      devices_(std::move(devices)),
      entry_point_(entry_point),
      target_url_(target_url),
      target_title_(target_title) {}

SendTabToSelfContextMenuDelegate::~SendTabToSelfContextMenuDelegate() = default;

// static
std::u16string SendTabToSelfContextMenuDelegate::GetDeviceItemLabel(
    const TargetDeviceInfo& device) {
  return l10n_util::GetStringFUTF16(IDS_SEND_TAB_TO_SELF_DEVICE_LABEL,
                                    base::UTF8ToUTF16(device.device_name),
                                    device.GetLastActiveTimeForDisplay());
}

void SendTabToSelfContextMenuDelegate::PopulateSubmenu(
    ui::SimpleMenuModel* model) {
  switch (display_reason_) {
    case EntryPointDisplayReason::kOfferSignIn:
    case EntryPointDisplayReason::kOfferReauth: {
      model->AddTitleWithStringId(IDS_PROFILES_LOCAL_PROFILE_STATE);
      // The three-dot share menu (`kShareMenu`) uses sentence case on all
      // platforms, whereas context menus use Title Case on macOS for
      // consistency with macOS system menus. The existing sentence-case string
      // `IDS_SEND_TAB_TO_SELF_SIGN_IN_PROMO_BUTTON_LABEL` is reused to avoid
      // duplicate GRD strings.
      const int sign_in_string_id =
          entry_point_ == ShareEntryPoint::kShareMenu
              ? IDS_SEND_TAB_TO_SELF_SIGN_IN_PROMO_BUTTON_LABEL
              : IDS_CONTEXT_MENU_SEND_TAB_TO_SELF_SIGN_IN;
      model->AddItemWithStringIdAndIcon(
          IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_SIGN_IN, sign_in_string_id,
          ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled()
                  ? kAccountCircleFilledIcon
                  : vector_icons::kAccountCircleOldIcon,
              ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize));
      return;
    }
    case EntryPointDisplayReason::kInformNoTargetDevice: {
      model->AddTitleWithStringId(
          IDS_SEND_TAB_TO_SELF_NO_OTHER_DEVICE_FOUND_TITLE);
      const int sign_in_on_phone_string_id =
          entry_point_ == ShareEntryPoint::kShareMenu
              ? IDS_SEND_TAB_TO_SELF_SIGN_IN_ON_PHONE
              : IDS_PROFILE_MENU_SIGNIN_ON_PHONE_BUTTON_LABEL;
      model->AddItemWithStringIdAndIcon(
          IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_SIGN_IN,
          sign_in_on_phone_string_id,
          ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kMobileIcon
                                                : kHardwareSmartphoneOldIcon,
              ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize));
      return;
    }
    case EntryPointDisplayReason::kOfferFeature:
      break;
  }

  for (size_t i = 0; i < devices_.size(); ++i) {
    model->AddItem(IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE1 + i,
                   GetDeviceItemLabel(devices_[i]));
  }

  model->AddSeparator(ui::NORMAL_SEPARATOR);
  const int manage_devices_string_id =
      entry_point_ == ShareEntryPoint::kShareMenu
          ? IDS_SEND_TAB_TO_SELF_MANAGE_DEVICES
          : IDS_CONTEXT_MENU_SEND_TAB_TO_SELF_MANAGE_DEVICES;
  model->AddItemWithStringId(
      IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_MANAGE_DEVICES,
      manage_devices_string_id);
}

bool SendTabToSelfContextMenuDelegate::IsCommandIdEnabled(
    int command_id) const {
  return (command_id >= IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE1 &&
          command_id <= IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE_LAST) ||
         command_id == IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_MANAGE_DEVICES ||
         command_id == IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_SIGN_IN;
}

void SendTabToSelfContextMenuDelegate::ExecuteCommand(int command_id,
                                                      int event_flags) {
  if (!primary_web_contents_) {
    return;
  }

  if (command_id == IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_SIGN_IN) {
    ShowBubble(primary_web_contents_.get(), entry_point_);
    return;
  }

  if (command_id == IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_MANAGE_DEVICES) {
    OpenManageDevicesPage(
        Profile::FromBrowserContext(primary_web_contents_->GetBrowserContext()),
        ui::DispositionFromEventFlags(
            event_flags, WindowOpenDisposition::NEW_FOREGROUND_TAB));
    return;
  }

  if (command_id >= IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE1 &&
      command_id <= IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE_LAST) {
    // The command IDs are allocated sequentially. Calculate the array
    // index of the selected device by offsetting the command ID by the base ID.
    size_t device_index =
        command_id - IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE1;

    if (device_index >= devices_.size()) {
      return;
    }

    const base::Feature& stts_feature =
        (entry_point_ == ShareEntryPoint::kLinkMenu ||
         base::FeatureList::IsEnabled(
             send_tab_to_self::kSendTabToSelfEnhancedDesktopUIv2))
            ? send_tab_to_self::kSendTabToSelfEnhancedDesktopUIv2
            : send_tab_to_self::kSendTabToSelfEnhancedDesktopUI;

    UserEducationService::MaybeNotifyNewBadgeFeatureUsed(
        primary_web_contents_->GetBrowserContext(), stts_feature);

    RecordEntryPointInvoked(entry_point_);

    for (const base::WeakPtr<content::WebContents>& web_contents :
         web_contents_list_) {
      if (!web_contents) {
        continue;
      }
      SendTabToSelfPageHandler* handler =
          SendTabToSelfPageHandler::GetOrCreateForWebContents(
              web_contents.get());

      auto callback = (web_contents.get() == primary_web_contents_.get())
                          ? base::BindOnce(&OnSendTabToDeviceComplete,
                                           primary_web_contents_,
                                           devices_[device_index].device_name,
                                           devices_[device_index].form_factor)
                          : base::DoNothing();

      handler->SendTabToDevice(
          devices_[device_index].cache_guid,
          ResolveTargetUrl(target_url_, web_contents.get()),
          ResolveTargetTitle(target_title_, web_contents.get()),
          std::move(callback), entry_point_);
    }
  }
}

void SendTabToSelfContextMenuDelegate::OnMenuWillShow(
    ui::SimpleMenuModel* source) {
  if (!primary_web_contents_ ||
      display_reason_ != EntryPointDisplayReason::kOfferFeature) {
    return;
  }
  Profile* profile =
      Profile::FromBrowserContext(primary_web_contents_->GetBrowserContext());
  SendTabToSelfSyncService* service =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile);
  if (!service) {
    return;
  }

  size_t device_count =
      service->GetSendTabToSelfModel()->GetTargetDeviceInfoSortedList().size();
  RecordTargetDeviceCount(entry_point_, EntryPointDisplayReason::kOfferFeature,
                          device_count);
}

}  // namespace send_tab_to_self
