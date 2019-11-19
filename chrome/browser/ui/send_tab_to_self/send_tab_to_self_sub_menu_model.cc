// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_sub_menu_model.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_desktop_util.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "content/public/browser/web_contents.h"

namespace send_tab_to_self {

namespace {

// Each item of submenu has its unique command id. These ids should not be same
// with the command ids of items in the menumodel. The range of all command
// ID's used in SendTabToSelfSubMenuModel must be equal or larger than
// |SendTabToSelfSubMenuModel::kMinCommandId| and less than
// |SendTabToSelfSubMenuModel::kMaxCommandId|.
// We assume that the user doesn't have more than 10 devices, if someone has,
// then the device list will only show the first 10 lines.
const int kMaxDevicesShown = 10;

const int kShareTabCommandId = SendTabToSelfSubMenuModel::kMinCommandId;
const int kShareLinkCommandId = 2010;
static_assert(
    kShareLinkCommandId - kShareTabCommandId == kMaxDevicesShown,
    "The range of command id for sharing tab should be no more than 10.");
const int kMaxCommandId = SendTabToSelfSubMenuModel::kMaxCommandId;
static_assert(
    kMaxCommandId - kShareLinkCommandId == kMaxDevicesShown,
    "The range of command id for sharing link should be no more than 10.");

// Returns true if the command id identifies a non-link contextual menu item.
bool IsShareTabCommandId(int command_id) {
  return command_id >= kShareTabCommandId && command_id < kShareLinkCommandId;
}

// Returns true if the command id identifies a link contextual menu item.
bool IsShareLinkCommandId(int command_id) {
  return command_id >= kShareLinkCommandId && command_id < kMaxCommandId;
}

// Converts |command_id| of menu item to index in |valid_device_items_|.
int CommandIdToVectorIndex(int command_id) {
  if (IsShareTabCommandId(command_id)) {
    return command_id - kShareTabCommandId;
  }
  if (IsShareLinkCommandId(command_id)) {
    return command_id - kShareLinkCommandId;
  }
  return -1;
}

// Converts menu type to string.
const std::string MenuTypeToString(SendTabToSelfMenuType menu_type) {
  switch (menu_type) {
    case SendTabToSelfMenuType::kTab:
      return kTabMenu;
    case SendTabToSelfMenuType::kContent:
      return kContentMenu;
    case SendTabToSelfMenuType::kOmnibox:
      return kOmniboxMenu;
    case SendTabToSelfMenuType::kLink:
      return kLinkMenu;
    default:
      NOTREACHED();
  }
}

}  // namespace

struct SendTabToSelfSubMenuModel::ValidDeviceItem {
  ValidDeviceItem(const std::string& device_name, const std::string& cache_guid)
      : device_name(device_name), cache_guid(cache_guid) {}

  std::string device_name;
  std::string cache_guid;
};

SendTabToSelfSubMenuModel::SendTabToSelfSubMenuModel(
    content::WebContents* tab,
    SendTabToSelfMenuType menu_type)
    : SendTabToSelfSubMenuModel(tab, menu_type, GURL()) {}

SendTabToSelfSubMenuModel::SendTabToSelfSubMenuModel(
    content::WebContents* tab,
    SendTabToSelfMenuType menu_type,
    const GURL& link_url)
    : ui::SimpleMenuModel(this),
      tab_(tab),
      menu_type_(menu_type),
      link_url_(link_url) {
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  Build(profile);
}

SendTabToSelfSubMenuModel::~SendTabToSelfSubMenuModel() {}

bool SendTabToSelfSubMenuModel::IsCommandIdEnabled(int command_id) const {
  // Only valid device names are shown, so all items are enabled.
  return true;
}

void SendTabToSelfSubMenuModel::ExecuteCommand(int command_id,
                                               int event_flags) {
  int vector_index = CommandIdToVectorIndex(command_id);
  if (vector_index == -1) {
    return;
  }
  const ValidDeviceItem& item = valid_device_items_[vector_index];
  if (menu_type_ == SendTabToSelfMenuType::kLink) {
    // Is sharing a link from link menu.
    CreateNewEntry(tab_, item.device_name, item.cache_guid, link_url_);
  } else {
    // Is sharing a tab from tab menu, content menu or omnibox menu.
    CreateNewEntry(tab_, item.device_name, item.cache_guid);
  }

  RecordSendTabToSelfClickResult(MenuTypeToString(menu_type_),
                                 SendTabToSelfClickResult::kClickItem);
  return;
}

void SendTabToSelfSubMenuModel::OnMenuWillShow(ui::SimpleMenuModel* source) {
  RecordSendTabToSelfClickResult(MenuTypeToString(menu_type_),
                                 SendTabToSelfClickResult::kShowDeviceList);
  RecordSendTabToSelfDeviceCount(MenuTypeToString(menu_type_),
                                 valid_device_items_.size());
}

void SendTabToSelfSubMenuModel::Build(Profile* profile) {
  SendTabToSelfSyncService* service =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile);
  DCHECK(service);
  SendTabToSelfModel* model = service->GetSendTabToSelfModel();
  DCHECK(model);
  std::vector<TargetDeviceInfo> devices =
      model->GetTargetDeviceInfoSortedList();
  if (!devices.empty()) {
    int index = 0;
    for (const auto& item : devices) {
      if (index == kMaxDevicesShown) {
        return;
      }
      BuildDeviceItem(item.device_name, item.cache_guid, index++);
    }
  }
  return;
}

void SendTabToSelfSubMenuModel::BuildDeviceItem(const std::string& device_name,
                                                const std::string& cache_guid,
                                                int index) {
  ValidDeviceItem item(device_name, cache_guid);
  int command_id =
      ((menu_type_ == kTab) ? kShareTabCommandId : kShareLinkCommandId) + index;
  InsertItemAt(index, command_id, base::UTF8ToUTF16(device_name));
  valid_device_items_.push_back(item);
}

}  //  namespace send_tab_to_self
