// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_toolbar_icon_controller.h"

#include "base/i18n/message_formatter.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_service.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_bubble_controller.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/l10n/l10n_util.h"

namespace send_tab_to_self {

SendTabToSelfToolbarIconController::SendTabToSelfToolbarIconController(
    Profile* profile)
    : profile_(profile) {}

// static
SendTabToSelfToolbarIconController*
SendTabToSelfToolbarIconController::FromReceivingUiHandlerInstance(
    send_tab_to_self::ReceivingUiHandler* ptr) {
  return static_cast<SendTabToSelfToolbarIconController*>(ptr);
}

// static
bool SendTabToSelfToolbarIconController::CanShowOnBrowser(
    BrowserWindowInterface* bwi) {
  return bwi->GetType() == BrowserWindowInterface::TYPE_NORMAL;
}

void SendTabToSelfToolbarIconController::DisplayNewEntries(
    const std::vector<const SendTabToSelfEntry*>& new_entries) {
  // TODO(crbug.com/40180897): Any entries that were never shown are lost.
  // This is consistent with current behavior and we don't have UI for
  // showing multiple entries with this iteration.
  if (new_entries.empty()) {
    return;
  }

  // If the active browser matches `profile_`, show the toolbar icon.
  // Otherwise, we will store this entry and wait to show on the next active
  // appropriate browser.
  BrowserWindowInterface* const browser =
      ProfileBrowserCollection::GetForProfile(profile_)->GetLastActiveBrowser();
  if (browser && (browser->IsActive() || ignore_active_for_testing_) &&
      CanShowOnBrowser(browser)) {
    // TODO(crbug.com/488072250): Move this logic into a separate notification
    // handler class.
    if (base::FeatureList::IsEnabled(kSendTabToSelfAutoOpen)) {
      // Open the first tab in the foreground and all the others in the
      // background.
      OpenEntryInNewForegroundTab(profile_, *new_entries[0]);
      RecordAutoOpenOutcome(AutoOpenOutcome::kSuccess);
      for (size_t ii = 1; ii < new_entries.size(); ++ii) {
        OpenEntryInNewBackgroundTab(profile_, *new_entries[ii]);
        RecordAutoOpenOutcome(AutoOpenOutcome::kSuccess);
      }

      // Show a toast.
      ToastParams params(ToastId::kSendTabToSelfTabOpened);
      params.body_string_override =
          base::i18n::MessageFormatter::FormatWithNumberedArgs(
              l10n_util::GetStringUTF16(IDS_SEND_TAB_RECEIVE_TOAST_FOREGROUND),
              static_cast<int>(new_entries.size()),
              base::UTF8ToUTF16(new_entries[0]->GetDeviceName()));
      browser->GetFeatures()
          .toast_service()
          ->toast_controller()
          ->MaybeShowToast(std::move(params));
    } else {
      // Select semi-randomly the first new entry from the list because there is
      // no UI to show multiple entries.
      ShowToolbarButton(*new_entries[0], browser);
    }
    return;
  }
  StorePendingEntries(new_entries);
}

void SendTabToSelfToolbarIconController::StorePendingEntries(
    const std::vector<const SendTabToSelfEntry*>&
        new_entries_pending_notification) {
  CHECK(!new_entries_pending_notification.empty());
  const bool had_entry_pending_notification = !pending_entries_.empty();

  if (base::FeatureList::IsEnabled(kSendTabToSelfAutoOpen)) {
    pending_entries_.reserve(pending_entries_.size() +
                             new_entries_pending_notification.size());
    for (const auto* entry : new_entries_pending_notification) {
      pending_entries_.push_back(std::make_unique<SendTabToSelfEntry>(*entry));
      RecordAutoOpenOutcome(AutoOpenOutcome::kPending);
    }
  } else {
    pending_entries_.clear();
    pending_entries_.push_back(std::make_unique<SendTabToSelfEntry>(
        *new_entries_pending_notification.front()));
  }

  // Prevent adding the observer several times. This might happen when the
  // window is inactive and this method is called more than once (i.e. the
  // server sends multiple entry batches).
  if (!had_entry_pending_notification) {
    browser_collection_observer_.Observe(
        ProfileBrowserCollection::GetForProfile(profile_));
  }
}

void SendTabToSelfToolbarIconController::DismissEntries(
    const std::vector<std::string>& guids) {
  auto* model = SendTabToSelfSyncServiceFactory::GetForProfile(profile_)
                    ->GetSendTabToSelfModel();
  for (const std::string& guid : guids) {
    model->DismissEntry(guid);
  }
}

void SendTabToSelfToolbarIconController::OnBrowserActivated(
    BrowserWindowInterface* browser) {
  if (!CanShowOnBrowser(browser)) {
    // Skip if not on a normal browser window.
    return;
  }

  browser_collection_observer_.Reset();

  // Reset `pending_entries_` because it's used to determine if the
  // BrowserListObserver is added in `DisplayNewEntries()`.
  std::vector<std::unique_ptr<SendTabToSelfEntry>> entries =
      std::move(pending_entries_);

  if (!profile_ || entries.empty()) {
    return;
  }

  if (base::FeatureList::IsEnabled(kSendTabToSelfAutoOpen)) {
    for (const std::unique_ptr<SendTabToSelfEntry>& entry : entries) {
      base::WeakPtr<content::WebContents> opened_contents =
          OpenEntryInNewBackgroundTab(profile_, *entry);
      if (opened_contents) {
        latest_tabs_opened_in_background_.push_back(
            tabs::TabInterface::GetFromContents(opened_contents.get())
                ->GetWeakPtr());
      }
      RecordAutoOpenOutcome(AutoOpenOutcome::kOpenedPending);
    }
    // Show a toast (only if there are tabs that were successfully opened in
    // the background).
    if (!latest_tabs_opened_in_background_.empty()) {
      ToastParams params(ToastId::kSendTabToSelfTabsOpenedInBackground);
      // Only show the device name of the first tab. Note that the tabs might
      // have been sent from different devices, but it's not worth the extra
      // hassle to list them all.
      params.body_string_override =
          base::i18n::MessageFormatter::FormatWithNumberedArgs(
              l10n_util::GetStringUTF16(IDS_SEND_TAB_RECEIVE_TOAST_BACKGROUND),
              static_cast<int>(latest_tabs_opened_in_background_.size()),
              base::UTF8ToUTF16(entries[0]->GetDeviceName()));
      params.toast_close_callback = base::ScopedClosureRunner(
          base::BindOnce(&SendTabToSelfToolbarIconController::OnToastClosed,
                         weak_ptr_factory_.GetWeakPtr()));
      browser->GetFeatures()
          .toast_service()
          ->toast_controller()
          ->MaybeShowToast(std::move(params));
    }
  } else {
    ShowToolbarButton(*entries.back(), browser);
  }
}

void SendTabToSelfToolbarIconController::ShowToolbarButton(
    const SendTabToSelfEntry& entry,
    BrowserWindowInterface* browser) {
  CHECK(browser);
  PinnedToolbarActions* controller =
      browser->GetFeatures().pinned_toolbar_actions();
  CHECK(controller);

  controller->ShowActionEphemerallyInToolbar(kActionSendTabToSelf, true);
  auto anchor = controller->GetBubbleAnchor(kActionSendTabToSelf);
  CHECK(!anchor.IsNull());
  send_tab_to_self::SendTabToSelfToolbarBubbleController::From(browser)
      ->ShowBubble(entry, anchor);

  send_tab_to_self::RecordNotificationShown();
}

void SendTabToSelfToolbarIconController::SwitchToLatestTabsOpenedInBackground(
    BrowserWindowInterface* browser) {
  CHECK(base::FeatureList::IsEnabled(kSendTabToSelfAutoOpen));
  CHECK(browser);

  for (const base::WeakPtr<tabs::TabInterface>& tab :
       latest_tabs_opened_in_background_) {
    int index = browser->GetTabStripModel()->GetIndexOfTab(tab.get());
    if (index != TabStripModel::kNoTab) {
      browser->GetTabStripModel()->ActivateTabAt(index);
      return;
    }
  }
}

void SendTabToSelfToolbarIconController::OnToastClosed() {
  // Clear all the tabs that were opened in the background. It is possible for
  // another toast to be shown before this method is ever called for the first
  // toast, but that is very very rare, because this toast is shown for the
  // browser was not active when the tabs were received.
  latest_tabs_opened_in_background_.clear();
}

SendTabToSelfToolbarIconController::~SendTabToSelfToolbarIconController() =
    default;

}  // namespace send_tab_to_self
