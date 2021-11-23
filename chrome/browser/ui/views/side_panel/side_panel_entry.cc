// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"

SidePanelEntry::SidePanelEntry(
    std::u16string name,
    base::RepeatingCallback<std::unique_ptr<views::View>()>
        create_content_callback)
    : name_(std::move(name)),
      create_content_callback_(std::move(create_content_callback)) {
  DCHECK(create_content_callback_);
}

SidePanelEntry::~SidePanelEntry() = default;

std::unique_ptr<views::View> SidePanelEntry::CreateContent() {
  return create_content_callback_.Run();
}
