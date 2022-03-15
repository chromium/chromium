// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ENTRY_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ENTRY_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/observer_list.h"
#include "ui/base/models/image_model.h"
#include "ui/views/view.h"

class SidePanelEntryObserver;

// This class represents an entry inside the side panel. These are owned by
// a SidePanelRegistry (either a per-tab or a per-window registry).
class SidePanelEntry final {
 public:
  // Note this order matches that of the combobox options in the side panel.
  enum class Id { kReadingList, kBookmarks, kReaderMode, kSideSearch, kLens };

  // TODO(pbos): Add an icon ImageModel here.
  SidePanelEntry(Id id,
                 std::u16string name,
                 const ui::ImageModel icon,
                 base::RepeatingCallback<std::unique_ptr<views::View>()>
                     create_content_callback);
  SidePanelEntry(const SidePanelEntry&) = delete;
  SidePanelEntry& operator=(const SidePanelEntry&) = delete;
  ~SidePanelEntry();

  // Creates the content to be shown inside the side panel when this entry is
  // shown.
  std::unique_ptr<views::View> CreateContent();
  // Called when the entry has been shown in the side panel.
  void OnEntryShown();

  Id id() const { return id_; }
  const std::u16string& name() const { return name_; }
  const ui::ImageModel& icon() const { return icon_; }

  void AddObserver(SidePanelEntryObserver* observer);
  void RemoveObserver(SidePanelEntryObserver* observer);

 private:
  const Id id_;
  const std::u16string name_;
  const ui::ImageModel icon_;

  base::RepeatingCallback<std::unique_ptr<views::View>()>
      create_content_callback_;

  base::ObserverList<SidePanelEntryObserver> observers_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ENTRY_H_
