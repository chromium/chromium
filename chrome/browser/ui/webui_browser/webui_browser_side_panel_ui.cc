// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui_browser/webui_browser_side_panel_ui.h"

#include <optional>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_waiter.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/webui_browser/webui_browser_window.h"

WebUIBrowserSidePanelUI::WebUIBrowserSidePanelUI(Browser* browser)
    : SidePanelUIBase(browser) {
  SidePanelUtil::PopulateGlobalEntries(browser, GetWindowRegistry());
}

WebUIBrowserSidePanelUI::~WebUIBrowserSidePanelUI() = default;

void WebUIBrowserSidePanelUI::Close() {}

void WebUIBrowserSidePanelUI::Toggle(SidePanelEntryKey key,
                                     SidePanelOpenTrigger open_trigger) {}

void WebUIBrowserSidePanelUI::OpenInNewTab() {}

void WebUIBrowserSidePanelUI::UpdatePinState() {}

content::WebContents* WebUIBrowserSidePanelUI::GetWebContentsForTest(
    SidePanelEntryId id) {
  return nullptr;
}

void WebUIBrowserSidePanelUI::DisableAnimationsForTesting() {}

void WebUIBrowserSidePanelUI::SetNoDelaysForTesting(
    bool no_delays_for_testing) {}

void WebUIBrowserSidePanelUI::Close(bool suppress_animations) {}

void WebUIBrowserSidePanelUI::Show(
    const UniqueKey& input,
    std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger,
    bool suppress_animations) {
  // Side panel is not supported for non-normal browsers.
  if (!browser()->is_type_normal()) {
    return;
  }

  SidePanelEntry* entry = GetEntryForUniqueKey(input);
  if (current_key() && *current_key() == input) {
    waiter_->ResetLoadingEntryIfNecessary();

    // TODO(webium): Implement the following:
    // If the side panel is in the process of closing, show it instead.
    // if (browser_view_->unified_side_panel()->state() ==
    // SidePanel::State::kClosing) {
    // browser_view_->unified_side_panel()->Open(/*animated=*/true);
    // NotifyPinnedContainerOfActiveStateChange(entry->key(), true);
    // }
    return;
  }

  waiter_->WaitForEntry(
      entry, base::BindOnce(&WebUIBrowserSidePanelUI::PopulateSidePanel,
                            base::Unretained(this), suppress_animations, input,
                            /*open_trigger=*/std::nullopt));
}

void WebUIBrowserSidePanelUI::PopulateSidePanel(
    bool suppress_animations,
    const UniqueKey& unique_key,
    std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger,
    SidePanelEntry* entry,
    std::optional<std::unique_ptr<views::View>> content_view) {
  SidePanelEntry* previous_entry = current_entry().get();
  if (previous_entry) {
    previous_entry->OnEntryWillHide(SidePanelEntryHideReason::kReplaced);

    // TODO(webium): Call previous_entry->OnEntryHidden() below when the entry
    // is swapped.

    previous_entry->CacheView(std::move(current_side_panel_view_));
    current_side_panel_view_.reset();
  }

  current_side_panel_view_ =
      content_view ? std::move(content_view.value()) : entry->GetContent();
  set_current_key(unique_key);
  set_current_entry(entry->GetWeakPtr());

  // TODO(webium) Implement WebUIBrowserWindow::ShowSidePanel() and call it.
  // GetWebUIBrowserWindow()->ShowSidePanel(entry->key());

  if (auto* contextual_registry = GetActiveContextualRegistry()) {
    contextual_registry->ResetActiveEntry();
  }

  entry->OnEntryShown();

  // TODO(webium): notify previous Entry::OnEntryHidden() if applicable.

  // TODO(webium): lens side panel will be closed on opening other side panels.
  // It observes the side panel opening using
  // SidePanelCoordinator::RegisterSidePanelShown().
}

void WebUIBrowserSidePanelUI::MaybeShowEntryOnTabStripModelChanged(
    SidePanelRegistry* old_contextual_registry,
    SidePanelRegistry* new_contextual_registry) {}

WebUIBrowserWindow* WebUIBrowserSidePanelUI::GetWebUIBrowserWindow() {
  return static_cast<WebUIBrowserWindow*>(browser()->window());
}
