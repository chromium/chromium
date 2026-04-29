// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/side_panel_ui_base.h"

#include "base/functional/callback_forward.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_waiter.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "content/public/browser/web_contents.h"

namespace {

SidePanelRegistry* GetSidePanelRegistryFromWebContents(
    content::WebContents* web_contents) {
  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(web_contents);
  if (!tab || !tab->GetTabFeatures()) {
    return nullptr;
  }
  return SidePanelRegistry::From(tab);
}

SidePanelRegistry* GetSidePanelRegistryFromTabHandle(tabs::TabHandle handle) {
  tabs::TabInterface* tab = handle.Get();
  if (!tab || !tab->GetTabFeatures()) {
    return nullptr;
  }
  return SidePanelRegistry::From(tab);
}

}  // namespace

SidePanelUIBase::PanelData::PanelData()
    : waiter(std::make_unique<SidePanelEntryWaiter>()) {}
SidePanelUIBase::PanelData::~PanelData() = default;

SidePanelUIBase::SidePanelUIBase(BrowserWindowInterface* browser)
    : browser_(browser) {
  panel_data_ = std::make_unique<PanelData>();
}

SidePanelUIBase::~SidePanelUIBase() = default;

void SidePanelUIBase::Show(SidePanelEntry::Id entry_id,
                           std::optional<SidePanelOpenTrigger> open_trigger,
                           bool suppress_animations) {
  Show(SidePanelEntry::Key(entry_id), open_trigger, suppress_animations);
}

void SidePanelUIBase::Show(SidePanelEntry::Key entry_key,
                           std::optional<SidePanelOpenTrigger> open_trigger,
                           bool suppress_animations) {
  std::optional<UniqueKey> unique_key = GetUniqueKeyForKey(entry_key);
  CHECK(unique_key.has_value());
  Show(unique_key.value(), open_trigger, suppress_animations);
}

std::optional<SidePanelEntry::Id> SidePanelUIBase::GetCurrentEntryId() const {
  if (!IsSidePanelShowing()) {
    return std::nullopt;
  }
  return current_key()->key.id();
}

int SidePanelUIBase::GetCurrentEntryDefaultContentWidth() const {
  if (!current_key().has_value()) {
    return SidePanelEntry::kSidePanelDefaultContentWidth;
  }

  const SidePanelEntry* const entry = GetEntryForUniqueKey(*current_key());
  CHECK(entry);

  return entry->GetDefaultContentWidth();
}

bool SidePanelUIBase::IsSidePanelShowing() const {
  return current_key().has_value();
}

bool SidePanelUIBase::IsSidePanelEntryShowing(
    const SidePanelEntry::Key& entry_key) const {
  return panel_data_->current_key.has_value() &&
         panel_data_->current_key->key == entry_key;
}

bool SidePanelUIBase::IsSidePanelEntryShowing(
    const SidePanelEntry::Key& entry_key,
    bool for_tab) const {
  return panel_data_->current_key.has_value() &&
         panel_data_->current_key->key == entry_key &&
         panel_data_->current_key->tab_handle.has_value() == for_tab;
}

base::CallbackListSubscription SidePanelUIBase::RegisterSidePanelShown(
    ShownCallback callback) {
  return panel_data_->shown_callback_list.Add(std::move(callback));
}

void SidePanelUIBase::SetOpenedTimestamp(base::TimeTicks timestamp) {
  panel_data_->opened_timestamp = timestamp;
}

void SidePanelUIBase::NotifyShownCallbacks() {
  panel_data_->shown_callback_list.Notify();
}

void SidePanelUIBase::SetCurrentKey(std::optional<UniqueKey> new_key) {
  panel_data_->current_key = new_key;
}

std::optional<SidePanelUIBase::UniqueKey> SidePanelUIBase::GetUniqueKeyForKey(
    const SidePanelEntry::Key& entry_key) const {
  // For tab-scoped side panels.
  if (GetActiveContextualRegistry() &&
      GetActiveContextualRegistry()->GetEntryForKey(entry_key)) {
    return UniqueKey{
        TabListInterface::From(browser_)->GetActiveTab()->GetHandle(),
        entry_key};
  }

  // For window-scoped side panels.
  if (SidePanelRegistry::From(browser_)->GetEntryForKey(entry_key)) {
    return UniqueKey{/*tab_handle=*/std::nullopt, entry_key};
  }
  return std::nullopt;
}

SidePanelEntry* SidePanelUIBase::GetEntryForUniqueKey(
    const UniqueKey& unique_key) const {
  SidePanelEntry* entry = nullptr;
  if (unique_key.tab_handle) {
    SidePanelRegistry* tab_registry =
        GetSidePanelRegistryFromTabHandle(unique_key.tab_handle.value());
    if (tab_registry) {
      entry = tab_registry->GetEntryForKey(unique_key.key);
    }
  } else {
    entry = SidePanelRegistry::From(browser_)->GetEntryForKey(unique_key.key);
  }
  return entry;
}

SidePanelRegistry* SidePanelUIBase::GetActiveContextualRegistry() const {
  if (TabListInterface::From(browser_)->GetTabCount() == 0) {
    return nullptr;
  }

  return SidePanelRegistry::From(
      TabListInterface::From(browser_)->GetActiveTab());
}

SidePanelEntry* SidePanelUIBase::GetActiveContextualEntryForKey(
    const SidePanelEntry::Key& entry_key) const {
  if (auto* contextual_registry = GetActiveContextualRegistry()) {
    return contextual_registry->GetEntryForKey(entry_key);
  }
  return nullptr;
}

std::optional<SidePanelUIBase::UniqueKey>
SidePanelUIBase::GetNewActiveKeyOnTabChanged() {
  // This function should only be called when the side panel view is shown.
  CHECK(IsSidePanelShowing());

  // Attempt to return an entry in the following fallback order:
  //  - the new tab's registry's active entry
  //  - if the active entry's key is registered in the global registry:
  //    - the new tab's registry's entry with the same key
  //    - the global registry's entry with the same key (note that
  //      GetEntryForKey will return this fallback order)
  //  - if there is an active entry in the global registry:
  //    - the new tab's registry's entry with the same key
  //    - the global registry's active entry (note that GetEntryForKey will
  //      return this fallback order)
  //  - no entry (this closes the side panel)
  // Note: GetActiveContextualRegistry() returns the registry for the new tab in
  // this function.
  // Note: If Show() is called with an entry returned by this function, then
  // that entry will be active in its owning registry.
  auto* active_contextual_registry = GetActiveContextualRegistry();
  if (active_contextual_registry &&
      active_contextual_registry->GetActiveEntry()) {
    return UniqueKey{
        TabListInterface::From(browser_)->GetActiveTab()->GetHandle(),
        (*active_contextual_registry->GetActiveEntry())->key()};
  }

  std::optional<UniqueKey> current_key = this->current_key();
  SidePanelRegistry* const window_registry = SidePanelRegistry::From(browser_);
  if (current_key && window_registry->GetEntryForKey(current_key->key)) {
    return GetUniqueKeyForKey(current_key->key);
  }

  if (auto entry = window_registry->GetActiveEntry()) {
    return GetUniqueKeyForKey((*entry)->key());
  }

  return std::nullopt;
}

SidePanelEntryWaiter* SidePanelUIBase::waiter() const {
  return panel_data_->waiter.get();
}

void SidePanelUIBase::OnActiveTabChanged(content::WebContents* old_contents,
                                         content::WebContents* new_contents,
                                         bool tab_removed_for_deletion) {
  SidePanelRegistry* old_contextual_registry = nullptr;
  if (!tab_removed_for_deletion && old_contents) {
    old_contextual_registry = GetSidePanelRegistryFromWebContents(old_contents);
  }

  // Add the current tab's contextual registry.
  SidePanelRegistry* new_contextual_registry = nullptr;
  if (new_contents) {
    new_contextual_registry = GetSidePanelRegistryFromWebContents(new_contents);
  }

  MaybeShowEntryOnTabStripModelChanged(old_contextual_registry,
                                       new_contextual_registry);
}
