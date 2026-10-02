// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_SUB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_SUB_MENU_MODEL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "chrome/app/chrome_command_ids.h"
#include "components/send_tab_to_self/entry_point_display_reason.h"
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

// A submenu model that builds and handles Send Tab to Self items in menus.
class SendTabToSelfSubMenuModel : public ui::SimpleMenuModel,
                                  public ui::SimpleMenuModel::Delegate {
 public:
  // Single-tab flow (e.g., page or hyperlink context menu, or Save and Share
  // app menu). Accepts optional `target_url` and `target_title` (e.g., link
  // anchor text). Returns nullptr if the preconditions for showing the submenu
  // are not met.
  static std::unique_ptr<SendTabToSelfSubMenuModel> MaybeCreateForTab(
      content::WebContents* web_contents,
      ShareEntryPoint entry_point,
      const GURL& target_url = GURL(),
      const std::string& target_title = std::string());

  // Multi-tab flow (e.g., tab strip context menu for multiple selected tabs).
  // Target URL/title are not applicable here as each tab resolves its own
  // URL/title. Returns nullptr if the preconditions for showing the submenu are
  // not met.
  static std::unique_ptr<SendTabToSelfSubMenuModel> MaybeCreateForMultipleTabs(
      content::WebContents* primary_web_contents,
      base::span<content::WebContents* const> web_contents_list,
      ShareEntryPoint entry_point);

  SendTabToSelfSubMenuModel(const SendTabToSelfSubMenuModel&) = delete;
  SendTabToSelfSubMenuModel& operator=(const SendTabToSelfSubMenuModel&) =
      delete;

  ~SendTabToSelfSubMenuModel() override;

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  void OnMenuWillShow(ui::SimpleMenuModel* source) override;

 private:
  SendTabToSelfSubMenuModel(content::WebContents* primary_web_contents,
                            EntryPointDisplayReason display_reason,
                            std::vector<TargetDeviceInfo> devices,
                            ShareEntryPoint entry_point,
                            const GURL& target_url,
                            const std::string& target_title);

  // Populates the submenu items appropriate for `display_reason_`.
  void BuildMenu();

  base::WeakPtr<content::WebContents> primary_web_contents_;
  std::vector<base::WeakPtr<content::WebContents>> web_contents_list_;
  const EntryPointDisplayReason display_reason_;
  const std::vector<TargetDeviceInfo> devices_;
  const ShareEntryPoint entry_point_;
  const GURL target_url_;
  const std::string target_title_;
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_SUB_MENU_MODEL_H_
