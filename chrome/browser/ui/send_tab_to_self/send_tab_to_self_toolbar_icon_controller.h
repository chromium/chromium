// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_ICON_CONTROLLER_H_
#define CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_ICON_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions.h"
#include "components/send_tab_to_self/receiving_ui_handler.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"

class BrowserWindowInterface;
class Profile;
class ProfileBrowserCollection;

namespace tabs {
class TabInterface;
}

namespace send_tab_to_self {

// Controller for send tab to self's toolbar button that decides when to show
// or hide the icon from the toolbar.
class SendTabToSelfToolbarIconController
    : public send_tab_to_self::ReceivingUiHandler,
      public BrowserCollectionObserver {
 public:
  explicit SendTabToSelfToolbarIconController(Profile* profile);
  SendTabToSelfToolbarIconController(
      const SendTabToSelfToolbarIconController&) = delete;
  SendTabToSelfToolbarIconController& operator=(
      const SendTabToSelfToolbarIconController&) = delete;
  ~SendTabToSelfToolbarIconController() override;

  // Casts the ReceivingUiHandler to a SendTabToSelfToolbarIconController.
  // This is safe because on Desktop only SentTabToSelfToolbarIconController
  // implements ReceivingUiHandler.
  // NOTE: It is the caller's responsibility to make sure that the instance
  // actually is a SendTabToSelfToolbarIconController before calling this
  // function.
  static SendTabToSelfToolbarIconController* FromReceivingUiHandlerInstance(
      send_tab_to_self::ReceivingUiHandler* ptr);

  // Returns true if the toolbar button can be shown for the provided browser.
  static bool CanShowOnBrowser(BrowserWindowInterface* bwi);

  // Bypasses the browser activation check in DisplayNewEntries.
  // Needed because browser activation is flaky in tests, especially on Wayland.
  void set_ignore_active_for_testing(bool ignore) {
    ignore_active_for_testing_ = ignore;
  }

  // ReceivingUiHandler implementation.
  void DisplayNewEntries(
      base::span<const send_tab_to_self::SendTabToSelfEntry* const> new_entries)
      override;
  void DismissEntries(base::span<const std::string> guids) override;

  // BrowserCollectionObserver implementation
  void OnBrowserActivated(BrowserWindowInterface* browser) override;

  // Switches to the latest tabs received from the remote and opened in
  // background. This opens the first tab in the list of latest tabs opened in
  // background. This is used by the toast action button to switch to the latest
  // tabs opened.
  void SwitchToLatestTabsOpenedInBackground(BrowserWindowInterface* browser);

  // Called when the receiving notification toast is closed, either by timeout
  // or by the user.
  void OnToastClosed();

 private:
  void StorePendingEntries(base::span<const SendTabToSelfEntry* const>
                               new_entries_pending_notification);

  void ShowToolbarButton(const SendTabToSelfEntry& entry,
                         BrowserWindowInterface* browser = nullptr);

  // Callback for GetBubbleAnchorAsync() that shows the bubble once the anchor
  // is ready.
  void ShowBubbleWithAnchor(base::WeakPtr<BrowserWindowInterface> browser,
                            SendTabToSelfEntry entry,
                            BubbleAnchorResult anchor);

  const raw_ptr<Profile, DanglingUntriaged> profile_;

  // In the case that we cannot immediately display a new entry
  // (e.g. the active browser is incognito or a different profile), we store it
  // here and wait until an appropriate browser becomes active to display it.
  std::vector<std::unique_ptr<SendTabToSelfEntry>> pending_entries_;

  std::vector<base::WeakPtr<tabs::TabInterface>>
      latest_tabs_opened_in_background_;

  base::ScopedObservation<ProfileBrowserCollection, BrowserCollectionObserver>
      browser_collection_observer_{this};

  // If true, bypasses the browser activation check in DisplayNewEntries.
  bool ignore_active_for_testing_ = false;

  base::WeakPtrFactory<SendTabToSelfToolbarIconController> weak_ptr_factory_{
      this};
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_ICON_CONTROLLER_H_
