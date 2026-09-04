// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/send_tab_to_self_dynamic_menu.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_page_handler.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_manager.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/send_tab_to_self/entry_point_display_reason.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/sync_device_info/device_info.h"
#include "content/public/browser/web_contents.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/menus/simple_menu_model.h"

namespace {

constexpr size_t kMaxDevices = 5;

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

void OnSendTabToDeviceComplete(base::WeakPtr<content::WebContents> web_contents,
                               std::string_view device_name,
                               syncer::DeviceInfo::FormFactor form_factor,
                               send_tab_to_self::SendTabToSelfResult result) {
  if (!web_contents || !base::FeatureList::IsEnabled(
                           send_tab_to_self::kSendTabToSelfPostSendToast)) {
    return;
  }

  switch (result) {
    case send_tab_to_self::SendTabToSelfResult::kSuccess:
      send_tab_to_self::ShowTabSentSuccessToast(web_contents.get(), device_name,
                                                form_factor);
      break;
    case send_tab_to_self::SendTabToSelfResult::kSuccessThrottled:
      send_tab_to_self::ShowTabSentThrottledToast(web_contents.get(),
                                                  device_name, form_factor);
      break;
    case send_tab_to_self::SendTabToSelfResult::kFailureInvalidUrl:
    case send_tab_to_self::SendTabToSelfResult::kFailureNotTrackingMetadata:
    case send_tab_to_self::SendTabToSelfResult::kFailureCommitAttemptFailed:
    case send_tab_to_self::SendTabToSelfResult::kFailureCommitAttemptError:
    case send_tab_to_self::SendTabToSelfResult::kFailureSyncDisabled:
    case send_tab_to_self::SendTabToSelfResult::kFailureEntryRemoved:
    case send_tab_to_self::SendTabToSelfResult::kFailureCommitTimeout:
    case send_tab_to_self::SendTabToSelfResult::kFailureNoInternetConnection:
      send_tab_to_self::ShowTabSentFailure(web_contents.get(), result, GURL());
      break;
  }
}

}  // namespace

SendTabToSelfDynamicMenu::SendTabToSelfDynamicMenu(
    BrowserWindowInterface* browser)
    : browser_window_interface_(browser) {}

SendTabToSelfDynamicMenu::~SendTabToSelfDynamicMenu() = default;

// static
std::u16string SendTabToSelfDynamicMenu::GetDeviceItemLabel(
    const send_tab_to_self::TargetDeviceInfo& device) {
  return l10n_util::GetStringFUTF16(IDS_SEND_TAB_TO_SELF_DEVICE_LABEL,
                                    base::UTF8ToUTF16(device.device_name),
                                    device.GetLastActiveTimeForDisplay());
}

void SendTabToSelfDynamicMenu::BuildSendTabToSelfActions(
    actions::BaseAction* parent_item) {
  if (!parent_item || !browser_window_interface_) {
    return;
  }

  parent_item->ResetActionList();

  content::WebContents* web_contents =
      browser_window_interface_->GetTabStripModel()
          ? browser_window_interface_->GetTabStripModel()
                ->GetActiveWebContents()
          : nullptr;
  if (!web_contents) {
    return;
  }

  Profile* profile = browser_window_interface_->GetProfile();
  send_tab_to_self::SendTabToSelfSyncService* service =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile);
  if (!service || !service->GetSendTabToSelfModel()) {
    return;
  }

  std::vector<send_tab_to_self::TargetDeviceInfo> devices =
      service->GetSendTabToSelfModel()->GetTargetDeviceInfoSortedList();

  if (devices.size() > kMaxDevices) {
    devices.erase(devices.begin() + kMaxDevices, devices.end());
  }

  send_tab_to_self::RecordTargetDeviceCount(
      send_tab_to_self::ShareEntryPoint::kShareMenu,
      send_tab_to_self::EntryPointDisplayReason::kOfferFeature, devices.size());

  for (const auto& device : devices) {
    std::u16string label = GetDeviceItemLabel(device);
    ui::ImageModel icon = ui::ImageModel::FromVectorIcon(
        GetDeviceIcon(device.form_factor), ui::kColorMenuIcon,
        ui::SimpleMenuModel::kDefaultIconSize);

    parent_item->AddChild(
        actions::ActionItem::Builder()
            .SetText(label)
            .SetImage(icon)
            .SetProperty(ActionAppMenuManager::kDisplayTypeKey,
                         ActionAppMenuManager::DisplayType::kRow)
            .SetProperty(ActionAppMenuManager::kContainerColorKey,
                         ui::kColorMenuBackground)
            .SetInvokeActionCallback(base::BindRepeating(
                &SendTabToSelfDynamicMenu::ExecuteDeviceSelection,
                weak_ptr_factory_.GetWeakPtr(), device.cache_guid,
                device.device_name, device.form_factor))
            .Build());
  }

  parent_item->AddChild(ActionAppMenuManager::CreateDividerActionItem());

  parent_item->AddChild(
      actions::ActionItem::Builder()
          .SetText(
              l10n_util::GetStringUTF16(IDS_SEND_TAB_TO_SELF_MANAGE_DEVICES))
          .SetProperty(ActionAppMenuManager::kDisplayTypeKey,
                       ActionAppMenuManager::DisplayType::kRow)
          .SetProperty(ActionAppMenuManager::kContainerColorKey,
                       ui::kColorMenuBackground)
          .SetInvokeActionCallback(base::BindRepeating(
              &SendTabToSelfDynamicMenu::ExecuteManageDevices,
              weak_ptr_factory_.GetWeakPtr()))
          .Build());
}

void SendTabToSelfDynamicMenu::ExecuteDeviceSelection(
    const std::string& target_device_guid,
    const std::string& device_name,
    syncer::DeviceInfo::FormFactor form_factor,
    actions::ActionItem* item,
    actions::ActionInvocationContext context) {
  if (!browser_window_interface_ ||
      !browser_window_interface_->GetTabStripModel()) {
    return;
  }
  content::WebContents* web_contents =
      browser_window_interface_->GetTabStripModel()->GetActiveWebContents();
  if (!web_contents) {
    return;
  }

  Profile* profile = browser_window_interface_->GetProfile();
  UserEducationService::MaybeNotifyNewBadgeFeatureUsed(
      profile, send_tab_to_self::kSendTabToSelfEnhancedDesktopUIv2);

  send_tab_to_self::RecordEntryPointInvoked(
      send_tab_to_self::ShareEntryPoint::kShareMenu);

  send_tab_to_self::SendTabToSelfPageHandler* handler =
      send_tab_to_self::SendTabToSelfPageHandler::GetOrCreateForWebContents(
          web_contents);

  auto callback =
      base::BindOnce(&OnSendTabToDeviceComplete, web_contents->GetWeakPtr(),
                     device_name, form_factor);

  handler->SendTabToDevice(
      target_device_guid, web_contents->GetLastCommittedURL(),
      base::UTF16ToUTF8(web_contents->GetTitle()), std::move(callback),
      send_tab_to_self::ShareEntryPoint::kShareMenu);
}

void SendTabToSelfDynamicMenu::ExecuteManageDevices(
    actions::ActionItem* item,
    actions::ActionInvocationContext context) {
  if (!browser_window_interface_) {
    return;
  }
  Profile* profile = browser_window_interface_->GetProfile();
  if (profile) {
    send_tab_to_self::OpenManageDevicesPage(profile, /*event_flags=*/0);
  }
}
