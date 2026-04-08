// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_CONTEXT_MENU_DELEGATE_H_
#define CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_CONTEXT_MENU_DELEGATE_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "ui/menus/simple_menu_model.h"

namespace content {
class WebContents;
}

namespace send_tab_to_self {

// A delegate class to manage Send Tab to Self items in context menus.
// Acts as the ui::SimpleMenuModel::Delegate for the submenu.
class SendTabToSelfContextMenuDelegate : public ui::SimpleMenuModel::Delegate {
 public:
  explicit SendTabToSelfContextMenuDelegate(content::WebContents* web_contents);

  SendTabToSelfContextMenuDelegate(const SendTabToSelfContextMenuDelegate&) =
      delete;
  SendTabToSelfContextMenuDelegate& operator=(
      const SendTabToSelfContextMenuDelegate&) = delete;

  ~SendTabToSelfContextMenuDelegate() override;

  // Populates the given `model` with the device items and "Manage Devices"
  // item.
  void PopulateSubmenu(ui::SimpleMenuModel* model);

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  // Returns the list of target devices to show in the context menu.
  // The returned list is capped at `kMaxDevices`.
  std::vector<TargetDeviceInfo> GetDevicesForDisplay() const;

  // Returns the label to show for a device in the context menu.
  static std::u16string GetDeviceItemLabel(const TargetDeviceInfo& device);

  base::WeakPtr<content::WebContents> web_contents_;
  const std::vector<TargetDeviceInfo> devices_;
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_CONTEXT_MENU_DELEGATE_H_
