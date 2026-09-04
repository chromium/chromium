// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_SEND_TAB_TO_SELF_DYNAMIC_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_SEND_TAB_TO_SELF_DYNAMIC_MENU_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/sync_device_info/device_info.h"
#include "ui/actions/actions.h"

// Class to support the creation of the Send Tab to Self Submenu in the AppMenu.
class SendTabToSelfDynamicMenu {
 public:
  explicit SendTabToSelfDynamicMenu(BrowserWindowInterface* browser);
  SendTabToSelfDynamicMenu(const SendTabToSelfDynamicMenu&) = delete;
  SendTabToSelfDynamicMenu& operator=(const SendTabToSelfDynamicMenu&) = delete;
  ~SendTabToSelfDynamicMenu();

  base::WeakPtr<SendTabToSelfDynamicMenu> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void BuildSendTabToSelfActions(actions::BaseAction* parent_item);

 private:
  static std::u16string GetDeviceItemLabel(
      const send_tab_to_self::TargetDeviceInfo& device);

  void ExecuteDeviceSelection(const std::string& target_device_guid,
                              const std::string& device_name,
                              syncer::DeviceInfo::FormFactor form_factor,
                              actions::ActionItem* item,
                              actions::ActionInvocationContext context);

  void ExecuteManageDevices(actions::ActionItem* item,
                            actions::ActionInvocationContext context);

  raw_ptr<BrowserWindowInterface> browser_window_interface_;
  base::WeakPtrFactory<SendTabToSelfDynamicMenu> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_SEND_TAB_TO_SELF_DYNAMIC_MENU_H_
