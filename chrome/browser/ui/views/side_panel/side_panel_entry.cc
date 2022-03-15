// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"

#include "base/observer_list.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"

SidePanelEntry::SidePanelEntry(
    Id id,
    std::u16string name,
    const ui::ImageModel icon,
    base::RepeatingCallback<std::unique_ptr<views::View>()>
        create_content_callback)
    : id_(id),
      name_(std::move(name)),
      icon_(std::move(icon)),
      create_content_callback_(std::move(create_content_callback)) {
  DCHECK(create_content_callback_);
}

SidePanelEntry::~SidePanelEntry() = default;

std::unique_ptr<views::View> SidePanelEntry::CreateContent() {
  return create_content_callback_.Run();
}

void SidePanelEntry::OnEntryShown() {
  for (SidePanelEntryObserver& observer : observers_)
    observer.OnEntryShown(this);
}

void SidePanelEntry::AddObserver(SidePanelEntryObserver* observer) {
  observers_.AddObserver(observer);
}

void SidePanelEntry::RemoveObserver(SidePanelEntryObserver* observer) {
  observers_.RemoveObserver(observer);
}
