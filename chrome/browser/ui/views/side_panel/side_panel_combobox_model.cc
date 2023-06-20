// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_combobox_model.h"

#include "base/containers/cxx20_erase.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/side_panel/companion/companion_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "ui/base/models/combobox_model_observer.h"
#include "ui/views/style/typography.h"

SidePanelComboboxModel::Item::Item(SidePanelEntry::Key key,
                                   std::u16string text,
                                   ui::ImageModel icon)
    : key(std::move(key)), text(std::move(text)), icon(std::move(icon)) {}
SidePanelComboboxModel::Item::Item(const SidePanelComboboxModel::Item& other) =
    default;
SidePanelComboboxModel::Item& SidePanelComboboxModel::Item::operator=(
    const SidePanelComboboxModel::Item& other) = default;
SidePanelComboboxModel::Item::Item(SidePanelComboboxModel::Item&& other) =
    default;
SidePanelComboboxModel::Item& SidePanelComboboxModel::Item::operator=(
    SidePanelComboboxModel::Item&& other) = default;
SidePanelComboboxModel::Item::~Item() = default;

SidePanelComboboxModel::SidePanelComboboxModel(BrowserView* browser_view)
    : browser_view_(browser_view) {}
SidePanelComboboxModel::~SidePanelComboboxModel() = default;

void SidePanelComboboxModel::AddItem(SidePanelEntry* entry) {
  if (HasKey(entry->key())) {
    return;
  }

  // Update the icon color if possible.
  ui::ImageModel icon = entry->icon();
  if (entry->icon().IsVectorIcon()) {
    icon = ui::ImageModel::FromVectorIcon(
        *entry->icon().GetVectorIcon().vector_icon(), kColorSidePanelEntryIcon,
        /*icon_size=*/16);
  }
  entries_.emplace_back(entry->key(), entry->name(), icon);
  std::sort(entries_.begin(), entries_.end(), [](const auto& a, const auto& b) {
    return a.key.id() < b.key.id();
  });
  for (auto& observer : observers()) {
    observer.OnComboboxModelChanged(this);
  }
}

void SidePanelComboboxModel::RemoveItem(const SidePanelEntry::Key& entry_key) {
  if (base::EraseIf(entries_, [entry_key](Item entry) {
        return entry.key == entry_key;
      })) {
    for (auto& observer : observers()) {
      observer.OnComboboxModelChanged(this);
    }
  }
}

void SidePanelComboboxModel::AddItems(
    const std::vector<std::unique_ptr<SidePanelEntry>>& entries) {
  bool items_added = false;
  for (auto const& entry : entries) {
    if (!HasKey(entry->key())) {
      items_added = true;

      // Update the icon color if possible.
      ui::ImageModel icon = entry->icon();
      if (entry->icon().IsVectorIcon()) {
        icon = ui::ImageModel::FromVectorIcon(
            *entry->icon().GetVectorIcon().vector_icon(),
            kColorSidePanelEntryIcon,
            /*icon_size=*/16);
      }
      entries_.emplace_back(entry->key(), entry->name(), icon);
    }
  }

  if (items_added) {
    std::sort(entries_.begin(), entries_.end(),
              [](const auto& a, const auto& b) { return a.key < b.key; });
    for (auto& observer : observers()) {
      observer.OnComboboxModelChanged(this);
    }
  }
}

void SidePanelComboboxModel::RemoveItems(
    const std::vector<SidePanelEntry::Key>& keys) {
  bool items_erased = false;
  for (const auto& key : keys) {
    auto position = base::ranges::find(entries_, key, &Item::key);
    if (position != entries_.end()) {
      items_erased = true;
      entries_.erase(position);
    }
  }

  if (items_erased) {
    for (auto& observer : observers()) {
      observer.OnComboboxModelChanged(this);
    }
  }
}

SidePanelEntry::Key SidePanelComboboxModel::GetKeyAt(int index) const {
  return entries_[index].key;
}

void SidePanelComboboxModel::UpdateIconForEntry(SidePanelEntry* entry) {
  for (auto& item : entries_) {
    if (item.key == entry->key()) {
      item.icon = entry->icon();
      for (auto& observer : observers()) {
        observer.OnComboboxModelChanged(this);
      }
    }
  }
}

int SidePanelComboboxModel::GetIndexForKey(const SidePanelEntry::Key& key) {
  for (size_t index = 0; index < entries_.size(); index++) {
    if (entries_[index].key == key)
      return index;
  }
  // Default to the first entry if the id doesn't exist.
  return 0;
}

bool SidePanelComboboxModel::HasKey(const SidePanelEntry::Key& key) const {
  return base::ranges::find(entries_, key, &Item::key) != entries_.end();
}

int SidePanelComboboxModel::GetKeyCountForTesting(
    const SidePanelEntry::Key& key) const {
  return base::ranges::count(entries_, key, &Item::key);
}

size_t SidePanelComboboxModel::GetItemCount() const {
  return entries_.size();
}

std::u16string SidePanelComboboxModel::GetItemAt(size_t index) const {
  return entries_[index].text;
}

ui::ImageModel SidePanelComboboxModel::GetIconAt(size_t index) const {
  if (!IsItemEnabledAt(index)) {
    // TODO(crbug.com/1447841): Remove all companion related special case code
    // once a generalized path forward has been determined.
    // For now, only companion should be able to be disabled.
    CHECK(GetKeyAt(index) ==
          SidePanelEntry::Key(SidePanelEntry::Id::kSearchCompanion));
    return ui::ImageModel::FromVectorIcon(
        *entries_[index].icon.GetVectorIcon().vector_icon(),
        ui::kColorIconDisabled,
        /*icon_size=*/16);
  }
  return entries_[index].icon;
}

bool SidePanelComboboxModel::IsItemEnabledAt(size_t index) const {
  // TODO(crbug.com/1447841): Remove all companion related special case code
  // once a generalized path forward has been determined.
  if (GetKeyAt(index) ==
      SidePanelEntry::Key(SidePanelEntry::Id::kSearchCompanion)) {
    return companion::IsCompanionAvailableForCurrentActiveTab(
        browser_view_->browser());
  }
  return true;
}
