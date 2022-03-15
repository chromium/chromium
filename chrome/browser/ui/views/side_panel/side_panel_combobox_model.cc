// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_combobox_model.h"

#include <algorithm>

#include "base/strings/utf_string_conversions.h"
#include "ui/views/style/typography.h"

SidePanelComboboxModel::Item::Item(SidePanelEntry::Id id,
                                   std::u16string text,
                                   ui::ImageModel icon)
    : id(id), text(std::move(text)), icon(std::move(icon)) {}
SidePanelComboboxModel::Item::Item(const SidePanelComboboxModel::Item& other) =
    default;
SidePanelComboboxModel::Item& SidePanelComboboxModel::Item::operator=(
    const SidePanelComboboxModel::Item& other) = default;
SidePanelComboboxModel::Item::Item(SidePanelComboboxModel::Item&& other) =
    default;
SidePanelComboboxModel::Item& SidePanelComboboxModel::Item::operator=(
    SidePanelComboboxModel::Item&& other) = default;
SidePanelComboboxModel::Item::~Item() = default;

SidePanelComboboxModel::SidePanelComboboxModel() = default;
SidePanelComboboxModel::~SidePanelComboboxModel() = default;

void SidePanelComboboxModel::AddItem(SidePanelEntry* entry) {
  entries_.emplace_back(
      SidePanelComboboxModel::Item(entry->id(), entry->name(), entry->icon()));
  std::sort(entries_.begin(), entries_.end(),
            [](const auto& a, const auto& b) { return a.id < b.id; });
}

void SidePanelComboboxModel::AddItems(
    const std::vector<std::unique_ptr<SidePanelEntry>>& entries) {
  for (auto const& entry : entries) {
    entries_.emplace_back(SidePanelComboboxModel::Item(
        entry->id(), entry->name(), entry->icon()));
  }
  std::sort(entries_.begin(), entries_.end(),
            [](const auto& a, const auto& b) { return a.id < b.id; });
}

void SidePanelComboboxModel::RemoveItems(
    const std::vector<std::unique_ptr<SidePanelEntry>>& entries) {
  for (auto const& current_entry : entries) {
    SidePanelEntry::Id id = current_entry.get()->id();
    auto position = std::find_if(entries_.begin(), entries_.end(),
                                 [id](auto entry) { return entry.id == id; });
    if (position != entries_.end())
      entries_.erase(position);
  }
}

SidePanelEntry::Id SidePanelComboboxModel::GetIdAt(int index) const {
  return entries_[index].id;
}

int SidePanelComboboxModel::GetIndexForId(SidePanelEntry::Id id) {
  for (size_t index = 0; index < entries_.size(); index++) {
    if (entries_[index].id == id)
      return index;
  }
  // Default to the first entry if the id doesn't exist.
  return 0;
}

int SidePanelComboboxModel::GetItemCount() const {
  return entries_.size();
}

std::u16string SidePanelComboboxModel::GetItemAt(int index) const {
  return entries_[index].text;
}

ui::ImageModel SidePanelComboboxModel::GetIconAt(int index) const {
  return entries_[index].icon;
}
