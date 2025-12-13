// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_ui_base.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_waiter.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "content/public/browser/web_contents.h"

namespace {

SidePanelRegistry* GetSidePanelRegistryFromWebContents(
    content::WebContents* web_contents) {
  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(web_contents);
  if (!tab || !tab->GetTabFeatures()) {
    return nullptr;
  }
  return tab->GetTabFeatures()->side_panel_registry();
}

SidePanelRegistry* GetSidePanelRegistryFromTabHandle(tabs::TabHandle handle) {
  tabs::TabInterface* tab = handle.Get();
  if (!tab || !tab->GetTabFeatures()) {
    return nullptr;
  }
  return tab->GetTabFeatures()->side_panel_registry();
}

}  // namespace

SidePanelUIBase::PanelData::PanelData()
    : waiter(std::make_unique<SidePanelEntryWaiter>()) {}
SidePanelUIBase::PanelData::~PanelData() = default;

SidePanelUIBase::SidePanelUIBase(Browser* browser) : browser_(browser) {
  for (auto panel_type : SidePanelEntry::PanelTypes::All()) {
    panel_data_[panel_type] = std::make_unique<PanelData>();
  }
  browser_->tab_strip_model()->AddObserver(this);
}

SidePanelUIBase::~SidePanelUIBase() = default;

void SidePanelUIBase::Show(
    SidePanelEntry::Id entry_id,
    std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger,
    bool suppress_animations) {
  Show(SidePanelEntry::Key(entry_id), open_trigger, suppress_animations);
}

void SidePanelUIBase::Show(
    SidePanelEntry::Key entry_key,
    std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger,
    bool suppress_animations) {
  std::optional<UniqueKey> unique_key = GetUniqueKeyForKey(entry_key);
  CHECK(unique_key.has_value());
  Show(unique_key.value(), open_trigger, suppress_animations);
}

std::optional<SidePanelEntry::Id> SidePanelUIBase::GetCurrentEntryId(
    SidePanelEntry::PanelType panel_type) const {
  if (!IsSidePanelShowing(panel_type)) {
    return std::nullopt;
  }
  return current_key(panel_type)->key.id();
}

int SidePanelUIBase::GetCurrentEntryDefaultContentWidth(
    SidePanelEntry::PanelType type) const {
  if (!current_key(type).has_value()) {
    return SidePanelEntry::kSidePanelDefaultContentWidth;
  }

  const SidePanelEntry* const entry = GetEntryForUniqueKey(*current_key(type));
  CHECK(entry);

  return entry->GetDefaultContentWidth();
}

bool SidePanelUIBase::IsSidePanelShowing(SidePanelEntry::PanelType type) const {
  return current_key(type).has_value();
}

bool SidePanelUIBase::IsSidePanelEntryShowing(
    const SidePanelEntry::Key& entry_key) const {
  for (const auto& [_, panel_data] : panel_data_) {
    if (panel_data->current_key && panel_data->current_key->key == entry_key) {
      return true;
    }
  }
  return false;
}

bool SidePanelUIBase::IsSidePanelEntryShowing(
    const SidePanelEntry::Key& entry_key,
    bool for_tab) const {
  for (const auto& [_, panel_data] : panel_data_) {
    std::optional<UniqueKey> current_key = panel_data->current_key;
    if (current_key && current_key->key == entry_key &&
        current_key->tab_handle.has_value() == for_tab) {
      return true;
    }
  }
  return false;
}

base::CallbackListSubscription SidePanelUIBase::RegisterSidePanelShown(
    SidePanelEntry::PanelType type,
    ShownCallback callback) {
  return panel_data_[type]->shown_callback_list.Add(std::move(callback));
}

void SidePanelUIBase::SetOpenedTimestamp(SidePanelEntry::PanelType type,
                                         base::TimeTicks timestamp) {
  panel_data_.at(type)->opened_timestamp = timestamp;
}

void SidePanelUIBase::NotifyShownCallbacksFor(SidePanelEntry::PanelType type) {
  panel_data_[type]->shown_callback_list.Notify();
}

void SidePanelUIBase::SetCurrentKey(SidePanelEntry::PanelType type,
                                    std::optional<UniqueKey> new_key) {
  panel_data_[type]->current_key = new_key;
}

std::optional<SidePanelUIBase::UniqueKey> SidePanelUIBase::GetUniqueKeyForKey(
    const SidePanelEntry::Key& entry_key) const {
  // For tab-scoped side panels.
  if (GetActiveContextualRegistry() &&
      GetActiveContextualRegistry()->GetEntryForKey(entry_key)) {
    return UniqueKey{browser_->GetActiveTabInterface()->GetHandle(), entry_key};
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
  if (browser_->tab_strip_model()->empty()) {
    return nullptr;
  }
  return browser_->GetActiveTabInterface()
      ->GetTabFeatures()
      ->side_panel_registry();
}

SidePanelEntry* SidePanelUIBase::GetActiveContextualEntryForKey(
    const SidePanelEntry::Key& entry_key) const {
  if (auto* contextual_registry = GetActiveContextualRegistry()) {
    return contextual_registry->GetEntryForKey(entry_key);
  }
  return nullptr;
}

std::optional<SidePanelUIBase::UniqueKey>
SidePanelUIBase::GetNewActiveKeyOnTabChanged(SidePanelEntry::PanelType type) {
  // This function should only be called when the side panel view is shown.
  CHECK(IsSidePanelShowing(type));

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
      active_contextual_registry->GetActiveEntryFor(type)) {
    return UniqueKey{
        browser_->GetActiveTabInterface()->GetHandle(),
        (*active_contextual_registry->GetActiveEntryFor(type))->key()};
  }

  std::optional<UniqueKey> current_key = this->current_key(type);
  SidePanelRegistry* const window_registry = SidePanelRegistry::From(browser_);
  if (current_key && window_registry->GetEntryForKey(current_key->key)) {
    return GetUniqueKeyForKey(current_key->key);
  }

  if (auto entry = window_registry->GetActiveEntryFor(type)) {
    return GetUniqueKeyForKey((*entry)->key());
  }

  return std::nullopt;
}

SidePanelEntryWaiter* SidePanelUIBase::waiter(
    SidePanelEntry::PanelType type) const {
  return panel_data_.at(type)->waiter.get();
}

void SidePanelUIBase::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  // If the browser window is closing, do nothing.
  if (tab_strip_model->closing_all()) {
    return;
  }

  if (!selection.active_tab_changed()) {
    return;
  }

  // Only background tabs can be discarded. In this case, nothing needs to
  // happen.
  if (change.type() == TabStripModelChange::kReplaced) {
    return;
  }

  // Handle removing the previous tab's contextual registry if one exists.
  bool tab_removed_for_deletion =
      (change.type() == TabStripModelChange::kRemoved) &&
      (change.GetRemove()->contents[0].remove_reason ==
       TabStripModelChange::RemoveReason::kDeleted);
  SidePanelRegistry* old_contextual_registry = nullptr;
  if (!tab_removed_for_deletion && selection.old_contents) {
    old_contextual_registry =
        GetSidePanelRegistryFromWebContents(selection.old_contents);
  }

  // Add the current tab's contextual registry.
  SidePanelRegistry* new_contextual_registry = nullptr;
  if (selection.new_contents) {
    new_contextual_registry =
        GetSidePanelRegistryFromWebContents(selection.new_contents);
  }

  MaybeShowEntryOnTabStripModelChanged(old_contextual_registry,
                                       new_contextual_registry);
}
