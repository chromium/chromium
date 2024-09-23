// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_ICON_CONTROLLER_H_
#define CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_ICON_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/send_tab_to_self/receiving_ui_handler.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"

class Profile;

namespace send_tab_to_self {

class SendTabToSelfToolbarIconControllerDelegate;

// Controller for the SendTabToSelfToolbarIconView that decides when to show
// or hide the icon from the toolbar.
// Owned by send_tab_to_self::ReceivingUiHandlerRegistry.
class SendTabToSelfToolbarIconController
    : public send_tab_to_self::ReceivingUiHandler,
      public BrowserListObserver {
 public:
  explicit SendTabToSelfToolbarIconController(Profile* profile);
  SendTabToSelfToolbarIconController(
      const SendTabToSelfToolbarIconController&) = delete;
  SendTabToSelfToolbarIconController& operator=(
      const SendTabToSelfToolbarIconController&) = delete;
  ~SendTabToSelfToolbarIconController() override;

  // ReceivingUiHandler implementation.
  void DisplayNewEntries(
      const std::vector<const send_tab_to_self::SendTabToSelfEntry*>&
          new_entries) override;
  void DismissEntries(const std::vector<std::string>& guids) override;

  // BrowserListObserver implementation
  void OnBrowserSetLastActive(Browser* browser) override;


  void AddDelegate(SendTabToSelfToolbarIconControllerDelegate* delegate);

  void RemoveDelegate(SendTabToSelfToolbarIconControllerDelegate* delegate);

  const Profile* profile() const override;

  void LogNotificationOpened();

  void LogNotificationDismissed();

  void ClearDelegateListForTesting() { delegate_list_.clear(); }

 private:
  void StorePendingEntry(
      const SendTabToSelfEntry* new_entry_pending_notification);

  void ShowToolbarButton(const SendTabToSelfEntry& entry,
                         Browser* browser = nullptr);

  raw_ptr<Profile, DanglingUntriaged> profile_;

  // In the case that we cannot immediately display a new entry
  // (e.g. the active browser is incognito or a different profile), we store it
  // here and wait until an appropriate browser becomes active to display it.
  std::unique_ptr<SendTabToSelfEntry> pending_entry_;

  std::vector<raw_ptr<SendTabToSelfToolbarIconControllerDelegate>>
      delegate_list_;

  SendTabToSelfToolbarIconControllerDelegate* GetActiveDelegate();
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_ICON_CONTROLLER_H_
