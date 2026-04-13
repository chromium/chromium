// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_toolbar_icon_controller.h"

#include "chrome/browser/platform_util.h"
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
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"

namespace send_tab_to_self {

SendTabToSelfToolbarIconController::SendTabToSelfToolbarIconController(
    Profile* profile)
    : profile_(profile) {}

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

  // Select semi-randomly the first new entry from the list because there is no
  // UI to show multiple entries.
  const SendTabToSelfEntry* new_entry = new_entries.front();

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
      OpenEntryInNewForegroundTab(profile_, *new_entry);
      // Show a toast.
      ToastParams params(ToastId::kSendTabToSelfTabOpened);
      params.body_string_replacement_params = {
          base::UTF8ToUTF16(new_entry->GetDeviceName())};
      browser->GetFeatures()
          .toast_service()
          ->toast_controller()
          ->MaybeShowToast(std::move(params));
      RecordAutoOpenOutcome(AutoOpenOutcome::kSuccess);
    } else {
      ShowToolbarButton(*new_entry, browser);
    }
    return;
  }
  if (base::FeatureList::IsEnabled(kSendTabToSelfAutoOpen)) {
    RecordAutoOpenOutcome(AutoOpenOutcome::kPending);
  }
  StorePendingEntry(new_entry);
}

void SendTabToSelfToolbarIconController::StorePendingEntry(
    const SendTabToSelfEntry* new_entry_pending_notification) {
  const bool had_entry_pending_notification = pending_entry_ != nullptr;

  // |pending_entry_| might already be set, but it's better to overwrite
  // it with a fresher value.
  // TODO(crbug.com/488072250): Allow multiple pending entries.
  pending_entry_ =
      std::make_unique<SendTabToSelfEntry>(*new_entry_pending_notification);
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
  browser_collection_observer_.Reset();

  // Reset |pending_entry_| because it's used to determine if the
  // BrowserListObserver is added in `DisplayNewEntries()`.
  std::unique_ptr<SendTabToSelfEntry> entry = std::move(pending_entry_);

  if (!profile_ || !entry) {
    return;
  }

  if (CanShowOnBrowser(browser)) {
    if (base::FeatureList::IsEnabled(kSendTabToSelfAutoOpen)) {
      OpenEntryInNewBackgroundTab(profile_, *entry);
      // TODO(crbug.com/488072250): Show a toast.
      RecordAutoOpenOutcome(AutoOpenOutcome::kOpenedPending);
    } else {
      ShowToolbarButton(*entry, browser);
    }
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

SendTabToSelfToolbarIconController::~SendTabToSelfToolbarIconController() =
    default;

}  // namespace send_tab_to_self
