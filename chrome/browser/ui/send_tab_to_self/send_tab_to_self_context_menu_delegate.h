// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_CONTEXT_MENU_DELEGATE_H_
#define CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_CONTEXT_MENU_DELEGATE_H_

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/app/chrome_command_ids.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "ui/menus/simple_menu_model.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace send_tab_to_self {

// The maximum number of target devices to show.
inline constexpr size_t kMaxDevices = 5;
inline constexpr int IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE_LAST =
    IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE1 + kMaxDevices - 1;

// A delegate class to manage Send Tab to Self items in context menus.
// Acts as the ui::SimpleMenuModel::Delegate for the submenu.
class SendTabToSelfContextMenuDelegate : public ui::SimpleMenuModel::Delegate {
 public:
  // Single-tab flow (e.g., page or hyperlink context menu).
  // Accepts optional `target_url` and `target_title` (e.g., link anchor text).
  SendTabToSelfContextMenuDelegate(
      content::WebContents* web_contents,
      ShareEntryPoint entry_point,
      const GURL& target_url = GURL(),
      const std::string& target_title = std::string());

  // Multi-tab flow (e.g., tab strip context menu for multiple selected tabs).
  // Target URL/title are not applicable here as each tab resolves its own
  // URL/title.
  SendTabToSelfContextMenuDelegate(
      content::WebContents* primary_web_contents,
      base::span<content::WebContents* const> web_contents_list,
      ShareEntryPoint entry_point);

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
  void OnMenuWillShow(ui::SimpleMenuModel* source) override;

 private:
  // Returns the list of target devices to show in the context menu.
  // The returned list is capped at `kMaxDevices`.
  std::vector<TargetDeviceInfo> GetDevicesForDisplay() const;

  // Returns the label to show for a device in the context menu.
  static std::u16string GetDeviceItemLabel(const TargetDeviceInfo& device);

  base::WeakPtr<content::WebContents> primary_web_contents_;
  std::vector<base::WeakPtr<content::WebContents>> web_contents_list_;
  const std::vector<TargetDeviceInfo> devices_;
  const ShareEntryPoint entry_point_;
  const GURL target_url_;
  const std::string target_title_;
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_CONTEXT_MENU_DELEGATE_H_
