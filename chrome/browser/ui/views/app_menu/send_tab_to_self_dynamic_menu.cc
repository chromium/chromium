// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/send_tab_to_self_dynamic_menu.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_page_handler.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_properties.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/app_menu/app_menu_action_item.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/send_tab_to_self/entry_point_display_reason.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/sync_device_info/device_info.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition.h"
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

content::WebContents* GetActiveWebContents(BrowserWindowInterface* browser) {
  if (tabs::TabInterface* active_tab = browser->GetActiveTabInterface()) {
    return active_tab->GetContents();
  }
  return nullptr;
}

}  // namespace

SendTabToSelfDynamicMenu::SendTabToSelfDynamicMenu(
    BrowserWindowInterface* browser)
    : browser_window_interface_(browser) {
  CHECK(browser_window_interface_);
}

SendTabToSelfDynamicMenu::~SendTabToSelfDynamicMenu() = default;

// static
std::u16string SendTabToSelfDynamicMenu::GetDeviceItemLabel(
    const send_tab_to_self::TargetDeviceInfo& device) {
  return l10n_util::GetStringFUTF16(IDS_SEND_TAB_TO_SELF_DEVICE_LABEL,
                                    base::UTF8ToUTF16(device.device_name),
                                    device.GetLastActiveTimeForDisplay());
}

bool SendTabToSelfDynamicMenu::ShouldShowSubmenu() const {
  if (!base::FeatureList::IsEnabled(
          send_tab_to_self::kSendTabToSelfEnhancedDesktopUIv2)) {
    return false;
  }

  std::optional<send_tab_to_self::EntryPointDisplayReason> reason =
      send_tab_to_self::GetEntryPointDisplayReason(
          GetActiveWebContents(browser_window_interface_));
  if (!reason.has_value()) {
    return false;
  }

  switch (*reason) {
    case send_tab_to_self::EntryPointDisplayReason::kOfferFeature: {
      send_tab_to_self::SendTabToSelfSyncService* service =
          SendTabToSelfSyncServiceFactory::GetForProfile(
              browser_window_interface_->GetProfile());
      return !service->GetSendTabToSelfModel()
                  ->GetTargetDeviceInfoSortedList()
                  .empty();
    }
    case send_tab_to_self::EntryPointDisplayReason::kOfferSignIn:
    case send_tab_to_self::EntryPointDisplayReason::kOfferReauth:
      return base::FeatureList::IsEnabled(
          send_tab_to_self::kSendTabToSelfSubmenuSigninPromos);
    case send_tab_to_self::EntryPointDisplayReason::kInformNoTargetDevice:
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      return base::FeatureList::IsEnabled(
                 send_tab_to_self::kSendTabToSelfSubmenuSigninPromos) &&
             base::FeatureList::IsEnabled(
                 send_tab_to_self::kSendTabToSelfNoTargetDeviceQrCode);
#else
      return false;
#endif
  }
  NOTREACHED();
}

void SendTabToSelfDynamicMenu::BuildSendTabToSelfActions(
    actions::BaseAction* parent_item) {
  CHECK(parent_item);
  parent_item->ResetActionList();

  std::optional<send_tab_to_self::EntryPointDisplayReason> reason =
      send_tab_to_self::GetEntryPointDisplayReason(
          GetActiveWebContents(browser_window_interface_));
  if (!reason.has_value()) {
    return;
  }

  switch (*reason) {
    case send_tab_to_self::EntryPointDisplayReason::kOfferSignIn:
    case send_tab_to_self::EntryPointDisplayReason::kOfferReauth: {
      parent_item->AddChild(AppMenuActionItem::CreateHeader(
          l10n_util::GetStringUTF16(IDS_PROFILES_LOCAL_PROFILE_STATE)));
      parent_item->AddChild(
          actions::ActionItem::Builder()
              .SetText(l10n_util::GetStringUTF16(
                  IDS_SEND_TAB_TO_SELF_SIGN_IN_PROMO_BUTTON_LABEL))
              .SetImage(ui::ImageModel::FromVectorIcon(
                  features::IsRoundedIconsEnabled()
                      ? kAccountCircleFilledIcon
                      : vector_icons::kAccountCircleOldIcon,
                  ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize))
              .SetProperty(AppMenuActionItem::kDisplayTypeKey,
                           AppMenuActionItem::DisplayType::kRow)
              .SetProperty(AppMenuActionItem::kContainerColorKey,
                           ui::kColorMenuBackground)
              .SetInvokeActionCallback(
                  base::BindRepeating(&SendTabToSelfDynamicMenu::ExecuteSignIn,
                                      weak_ptr_factory_.GetWeakPtr()))
              .Build());
      return;
    }
    case send_tab_to_self::EntryPointDisplayReason::kInformNoTargetDevice: {
      parent_item->AddChild(
          AppMenuActionItem::CreateHeader(l10n_util::GetStringUTF16(
              IDS_SEND_TAB_TO_SELF_NO_OTHER_DEVICE_FOUND_TITLE)));
      parent_item->AddChild(
          actions::ActionItem::Builder()
              .SetText(l10n_util::GetStringUTF16(
                  IDS_SEND_TAB_TO_SELF_SIGN_IN_ON_PHONE))
              .SetImage(ui::ImageModel::FromVectorIcon(
                  features::IsRoundedIconsEnabled()
                      ? kMobileIcon
                      : kHardwareSmartphoneOldIcon,
                  ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize))
              .SetProperty(AppMenuActionItem::kDisplayTypeKey,
                           AppMenuActionItem::DisplayType::kRow)
              .SetProperty(AppMenuActionItem::kContainerColorKey,
                           ui::kColorMenuBackground)
              .SetInvokeActionCallback(
                  base::BindRepeating(&SendTabToSelfDynamicMenu::ExecuteSignIn,
                                      weak_ptr_factory_.GetWeakPtr()))
              .Build());
      return;
    }
    case send_tab_to_self::EntryPointDisplayReason::kOfferFeature:
      break;
  }

  send_tab_to_self::SendTabToSelfSyncService* service =
      SendTabToSelfSyncServiceFactory::GetForProfile(
          browser_window_interface_->GetProfile());

  std::vector<send_tab_to_self::TargetDeviceInfo> devices =
      service->GetSendTabToSelfModel()->GetTargetDeviceInfoSortedList();

  if (devices.size() > kMaxDevices) {
    devices.erase(devices.begin() + kMaxDevices, devices.end());
  }

  for (const auto& device : devices) {
    std::u16string label = GetDeviceItemLabel(device);
    ui::ImageModel icon = ui::ImageModel::FromVectorIcon(
        GetDeviceIcon(device.form_factor), ui::kColorMenuIcon,
        ui::SimpleMenuModel::kDefaultIconSize);

    parent_item->AddChild(
        actions::ActionItem::Builder()
            .SetText(label)
            .SetImage(icon)
            .SetProperty(AppMenuActionItem::kDisplayTypeKey,
                         AppMenuActionItem::DisplayType::kRow)
            .SetProperty(AppMenuActionItem::kContainerColorKey,
                         ui::kColorMenuBackground)
            .SetInvokeActionCallback(base::BindRepeating(
                &SendTabToSelfDynamicMenu::ExecuteDeviceSelection,
                weak_ptr_factory_.GetWeakPtr(), device.cache_guid,
                device.device_name, device.form_factor))
            .Build());
  }

  parent_item->AddChild(AppMenuActionItem::CreateDivider());

  parent_item->AddChild(actions::ActionItem::Builder()
                            .SetText(l10n_util::GetStringUTF16(
                                IDS_SEND_TAB_TO_SELF_MANAGE_DEVICES))
                            .SetProperty(AppMenuActionItem::kDisplayTypeKey,
                                         AppMenuActionItem::DisplayType::kRow)
                            .SetProperty(AppMenuActionItem::kContainerColorKey,
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
  content::WebContents* web_contents =
      GetActiveWebContents(browser_window_interface_);
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
  WindowOpenDisposition disposition =
      context.GetProperty(chrome::kDispositionKey);
  if (disposition == WindowOpenDisposition::CURRENT_TAB ||
      disposition == WindowOpenDisposition::UNKNOWN) {
    disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  }
  send_tab_to_self::OpenManageDevicesPage(
      browser_window_interface_->GetProfile(), disposition);
}

void SendTabToSelfDynamicMenu::ExecuteSignIn(
    actions::ActionItem* item,
    actions::ActionInvocationContext context) {
  content::WebContents* web_contents =
      GetActiveWebContents(browser_window_interface_);
  if (!web_contents) {
    return;
  }
  send_tab_to_self::ShowBubble(web_contents,
                               send_tab_to_self::ShareEntryPoint::kShareMenu);
}
