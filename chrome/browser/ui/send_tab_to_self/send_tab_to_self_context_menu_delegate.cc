// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_context_menu_delegate.h"

#include <string_view>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_page_handler.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition_utils.h"

namespace send_tab_to_self {

namespace {

constexpr size_t kMaxDevices = 5;

static_assert(IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE_LAST -
                      IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE1 + 1 ==
                  kMaxDevices,
              "kMaxDevices must match the number of command IDs reserved for "
              "target devices in chrome_command_ids.h");

void OnSendTabToDeviceComplete(base::WeakPtr<content::WebContents> web_contents,
                               std::string_view device_name,
                               SendTabToSelfResult result) {
  switch (result) {
    case SendTabToSelfResult::kSuccess:
    case SendTabToSelfResult::kSuccessThrottled:
      if (web_contents &&
          base::FeatureList::IsEnabled(kSendTabToSelfPostSendToast)) {
        ShowTabSentSuccessToast(web_contents.get(), device_name);
      }
      break;
    case SendTabToSelfResult::kFailureInvalidUrl:
    case SendTabToSelfResult::kFailureNotTrackingMetadata:
    case SendTabToSelfResult::kFailureCommitAttemptFailed:
    case SendTabToSelfResult::kFailureCommitAttemptError:
    case SendTabToSelfResult::kFailureSyncDisabled:
    case SendTabToSelfResult::kFailureEntryRemoved:
    case SendTabToSelfResult::kFailureCommitTimeout:
      break;
  }
}

}  // namespace

SendTabToSelfContextMenuDelegate::SendTabToSelfContextMenuDelegate(
    content::WebContents* web_contents)
    : web_contents_(web_contents ? web_contents->GetWeakPtr() : nullptr),
      devices_(GetDevicesForDisplay()) {}

SendTabToSelfContextMenuDelegate::~SendTabToSelfContextMenuDelegate() = default;

std::vector<TargetDeviceInfo>
SendTabToSelfContextMenuDelegate::GetDevicesForDisplay() const {
  if (!web_contents_) {
    return {};
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
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

// static
std::u16string SendTabToSelfContextMenuDelegate::GetDeviceItemLabel(
    const TargetDeviceInfo& device) {
  return l10n_util::GetStringFUTF16(IDS_SEND_TAB_TO_SELF_DEVICE_LABEL,
                                    base::UTF8ToUTF16(device.device_name),
                                    device.GetLastActiveTimeForDisplay());
}

void SendTabToSelfContextMenuDelegate::PopulateSubmenu(
    ui::SimpleMenuModel* model) {
  for (size_t i = 0; i < devices_.size(); ++i) {
    model->AddItem(IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE1 + i,
                   GetDeviceItemLabel(devices_[i]));
  }

  // TODO(crbug.com/488252159): Implement edge case handling (e.g., no devices
  // or not signed in) according to design spec.

  if (!devices_.empty()) {
    model->AddSeparator(ui::NORMAL_SEPARATOR);
  }
  model->AddItemWithStringId(
      IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_MANAGE_DEVICES,
      IDS_SEND_TAB_TO_SELF_MANAGE_DEVICES);
}

bool SendTabToSelfContextMenuDelegate::IsCommandIdEnabled(
    int command_id) const {
  return (command_id >= IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE1 &&
          command_id <= IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE_LAST) ||
         command_id == IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_MANAGE_DEVICES;
}

void SendTabToSelfContextMenuDelegate::ExecuteCommand(int command_id,
                                                      int event_flags) {
  if (!web_contents_) {
    return;
  }

  if (command_id == IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_MANAGE_DEVICES) {
    NavigateParams params(
        Profile::FromBrowserContext(web_contents_->GetBrowserContext()),
        GURL(chrome::kGoogleAccountDeviceActivityURL),
        ui::PAGE_TRANSITION_LINK);
    params.disposition = ui::DispositionFromEventFlags(
        event_flags, WindowOpenDisposition::NEW_FOREGROUND_TAB);
    Navigate(&params);
    return;
  }

  if (command_id >= IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE1 &&
      command_id <= IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE_LAST) {
    // The command IDs are allocated sequentially. We can calculate the array
    // index of the selected device by offsetting the command ID by the base ID.
    size_t device_index =
        command_id - IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE1;

    if (device_index >= devices_.size()) {
      return;
    }

    SendTabToSelfPageHandler* handler =
        SendTabToSelfPageHandler::GetOrCreateForWebContents(
            web_contents_.get());
    handler->SendTabToDevice(
        devices_[device_index].cache_guid, web_contents_->GetLastCommittedURL(),
        base::UTF16ToUTF8(web_contents_->GetTitle()),
        base::BindOnce(&OnSendTabToDeviceComplete, web_contents_,
                       devices_[device_index].device_name));
  }
}

}  // namespace send_tab_to_self
