// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_COMBOBOX_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_COMBOBOX_MODEL_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/image_model.h"

// A model for the side panel entry combobox. This is used to switch
// between available entries within the side panel. The SidePanelCoordinator
// will handle adding and removing side panel entries from this model as their
// availability changes.
class SidePanelComboboxModel : public ui::ComboboxModel {
 public:
  struct Item {
    Item(SidePanelEntry::Key key, std::u16string text, ui::ImageModel icon);
    Item(const Item& other);
    Item& operator=(const Item& other);
    Item(Item&& other);
    Item& operator=(Item&& other);
    ~Item();

    SidePanelEntry::Key key;
    std::u16string text;
    ui::ImageModel icon;
  };

  SidePanelComboboxModel();
  SidePanelComboboxModel(const SidePanelComboboxModel&) = delete;
  SidePanelComboboxModel& operator=(const SidePanelComboboxModel&) = delete;
  ~SidePanelComboboxModel() override;

  void AddItem(SidePanelEntry* entry);
  void RemoveItem(const SidePanelEntry::Key& entry_key);
  void AddItems(const std::vector<std::unique_ptr<SidePanelEntry>>& entries);
  void RemoveItems(const std::vector<SidePanelEntry::Key>& keys);
  SidePanelEntry::Key GetKeyAt(int index) const;

  // If the entry is in |entries_|, update the icon.
  void UpdateIconForEntry(SidePanelEntry* entry);

  // Returns the index for the given side panel entry key, if the key doesn't
  // exist in `entries_` then default to 0.
  int GetIndexForKey(const SidePanelEntry::Key& key);

  // Returns if the given side panel entry key exists in `entries_`.
  bool HasKey(const SidePanelEntry::Key& key) const;

  // Returns the number of `entries` with the given `key`. This should only be
  // used to sanity check that a given extension key occurs at most once.
  int GetKeyCountForTesting(const SidePanelEntry::Key& key) const;

  // ui::ComboboxModel:
  size_t GetItemCount() const override;
  std::u16string GetItemAt(size_t index) const override;
  ui::ImageModel GetIconAt(size_t index) const override;

 private:
  std::vector<Item> entries_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_COMBOBOX_MODEL_H_
