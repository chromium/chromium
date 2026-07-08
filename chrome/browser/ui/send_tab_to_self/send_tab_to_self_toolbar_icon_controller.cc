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
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_activation_tracker.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_service.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
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
    : profile_(profile) {
  if (base::FeatureList::IsEnabled(kSendTabToSelfAutoOpen)) {
    SendTabToSelfModel* model = GetModel();
    model_observation_.Observe(model);
    if (model->IsReady()) {
      OnModelReady();
    }
  }
}

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
    base::span<const send_tab_to_self::SendTabToSelfEntry* const> new_entries) {
  if (new_entries.empty()) {
    return;
  }

  if (base::FeatureList::IsEnabled(kSendTabToSelfAutoOpen)) {
    // If there is an active browser for this profile, open the tabs
    // immediately.
    if (BrowserWindowInterface* const browser = GetActiveBrowser()) {
      // Open the first tab in the foreground and all the others in the
      // background.
      OpenEntryInNewForegroundTab(profile_, *new_entries[0],
                                  ShareActivatedEntryPoint::kAutoOpened);
      RecordAutoOpenOutcome(AutoOpenOutcome::kTabOpenedInForeground);
      for (size_t ii = 1; ii < new_entries.size(); ++ii) {
        OpenEntryInNewBackgroundTab(profile_, *new_entries[ii]);
        RecordAutoOpenOutcome(
            AutoOpenOutcome::kTabsOpenedImmediatelyInBackground);
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
      // If no browser is active, record the entries as pending and wait for
      // a browser window to be activated.
      for (size_t ii = 0; ii < new_entries.size(); ++ii) {
        RecordAutoOpenOutcome(AutoOpenOutcome::kUnopenedImmediately);
      }
      StartObservingBrowserCollection();
    }
  } else {
    // If there is an active browser for this profile, show the toolbar button
    // immediately.
    if (BrowserWindowInterface* const browser = GetActiveBrowser()) {
      // Select semi-randomly the first new entry from the list because there is
      // no UI to show multiple entries.
      ShowToolbarButton(*new_entries[0], browser);
    } else {
      // Otherwise, store the entry and wait for a browser to be activated.
      pending_entry_ =
          std::make_unique<SendTabToSelfEntry>(*new_entries.front());
      StartObservingBrowserCollection();
    }
  }
}

void SendTabToSelfToolbarIconController::DismissEntries(
    base::span<const std::string> guids) {
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

  if (!base::FeatureList::IsEnabled(kSendTabToSelfAutoOpen)) {
    if (pending_entry_) {
      ShowToolbarButton(*pending_entry_, browser);
      pending_entry_.reset();
    }
    return;
  }

  SendTabToSelfModel* model = GetModel();
  std::vector<const SendTabToSelfEntry*> unopened_entries =
      model->GetUnopenedEntriesTargetedToLocalDevice();
  if (unopened_entries.empty()) {
    return;
  }
  for (const SendTabToSelfEntry* entry : unopened_entries) {
    base::WeakPtr<content::WebContents> opened_contents =
        OpenEntryInNewBackgroundTab(profile_, *entry);
    if (opened_contents) {
      latest_tabs_opened_in_background_.push_back(
          tabs::TabInterface::GetFromContents(opened_contents.get())
              ->GetWeakPtr());
    }
    RecordAutoOpenOutcome(
        AutoOpenOutcome::kTabsOpenedInBackgroundUponActivation);
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
            base::UTF8ToUTF16(unopened_entries[0]->GetDeviceName()));
    params.toast_close_callback = base::ScopedClosureRunner(
        base::BindOnce(&SendTabToSelfToolbarIconController::OnToastClosed,
                       weak_ptr_factory_.GetWeakPtr()));
    browser->GetFeatures().toast_service()->toast_controller()->MaybeShowToast(
        std::move(params));
  }
}

void SendTabToSelfToolbarIconController::OnModelReady() {
  CHECK(base::FeatureList::IsEnabled(kSendTabToSelfAutoOpen));
  SendTabToSelfModel* model = GetModel();
  if (!model->GetUnopenedEntriesTargetedToLocalDevice().empty()) {
    if (BrowserWindowInterface* const browser = GetActiveBrowser()) {
      OnBrowserActivated(browser);
    } else {
      StartObservingBrowserCollection();
    }
  }
}

void SendTabToSelfToolbarIconController::ShowToolbarButton(
    const SendTabToSelfEntry& entry,
    BrowserWindowInterface* browser) {
  CHECK(!base::FeatureList::IsEnabled(kSendTabToSelfAutoOpen));
  CHECK(browser);
  PinnedToolbarActions* controller =
      browser->GetFeatures().pinned_toolbar_actions();
  CHECK(controller);

  controller->ShowActionEphemerallyInToolbar(kActionSendTabToSelf, true);
  controller->GetBubbleAnchorAsync(
      kActionSendTabToSelf,
      base::BindOnce(&SendTabToSelfToolbarIconController::ShowBubbleWithAnchor,
                     weak_ptr_factory_.GetWeakPtr(), browser->GetWeakPtr(),
                     entry));
}

void SendTabToSelfToolbarIconController::ShowBubbleWithAnchor(
    base::WeakPtr<BrowserWindowInterface> browser,
    SendTabToSelfEntry entry,
    BubbleAnchorResult anchor) {
  if (!anchor.has_value()) {
    if (!browser) {
      return;
    }
    // PinnedToolbarActions failed to find an anchor. Try ToolbarButtonProvider
    // as it has fallback anchor logic.
    auto new_anchor = ToolbarButtonProvider::From(browser.get())
                          ->GetBubbleAnchor(kActionSendTabToSelf);
    if (new_anchor.IsNull()) {
      return;
    }
    anchor = new_anchor;
  }
  send_tab_to_self::SendTabToSelfToolbarBubbleController::From(browser.get())
      ->ShowBubble(entry, anchor.value());
  send_tab_to_self::RecordNotificationShown();
}

void SendTabToSelfToolbarIconController::SwitchToLatestTabsOpenedInBackground(
    BrowserWindowInterface* browser) {
  CHECK(base::FeatureList::IsEnabled(kSendTabToSelfAutoOpen));
  CHECK(browser);

  for (const base::WeakPtr<tabs::TabInterface>& tab :
       latest_tabs_opened_in_background_) {
    if (!tab || tab->GetBrowserWindowInterface() != browser) {
      continue;
    }
    SendTabToSelfActivationTracker::SetEntryOpenedViaToast(tab->GetContents());
    browser->GetTabStripModel()->ActivateTab(tab.get());
    return;
  }
}

void SendTabToSelfToolbarIconController::OnToastClosed() {
  // Clear all the tabs that were opened in the background. It is possible for
  // another toast to be shown before this method is ever called for the first
  // toast, but that is very very rare, because this toast is shown for the
  // browser was not active when the tabs were received.
  latest_tabs_opened_in_background_.clear();
}

BrowserWindowInterface* SendTabToSelfToolbarIconController::GetActiveBrowser() {
  BrowserWindowInterface* const browser =
      ProfileBrowserCollection::GetForProfile(profile_)->GetLastActiveBrowser();
  return (browser && (browser->IsActive() || ignore_active_for_testing_) &&
          CanShowOnBrowser(browser))
             ? browser
             : nullptr;
}

SendTabToSelfModel* SendTabToSelfToolbarIconController::GetModel() {
  return SendTabToSelfSyncServiceFactory::GetForProfile(profile_)
      ->GetSendTabToSelfModel();
}

void SendTabToSelfToolbarIconController::StartObservingBrowserCollection() {
  // Prevent adding the observer multiple times if this is called repeatedly
  // while the window is inactive (e.g. if the server sends multiple batches
  // of entries).
  if (!browser_collection_observer_.IsObserving()) {
    browser_collection_observer_.Observe(
        ProfileBrowserCollection::GetForProfile(profile_));
  }
}

SendTabToSelfToolbarIconController::~SendTabToSelfToolbarIconController() =
    default;

}  // namespace send_tab_to_self
