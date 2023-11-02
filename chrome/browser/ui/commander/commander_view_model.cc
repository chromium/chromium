// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commander/commander_view_model.h"

namespace commander {

CommandItemViewModel::CommandItemViewModel(
    const std::u16string& title,
    const std::vector<gfx::Range>& matched_ranges,
    CommandItem::Entity entity_type,
    const std::u16string& annotation)
    : title(title),
      matched_ranges(matched_ranges),
      entity_type(entity_type),
      annotation(annotation) {}

CommandItemViewModel::CommandItemViewModel(const CommandItem& item)
    : CommandItemViewModel(item.title,
                           item.matched_ranges,
                           item.entity_type,
                           item.annotation) {}
CommandItemViewModel::~CommandItemViewModel() = default;
CommandItemViewModel::CommandItemViewModel(const CommandItemViewModel& other) =
    default;

CommanderViewModel::CommanderViewModel() = default;
CommanderViewModel::~CommanderViewModel() = default;
CommanderViewModel::CommanderViewModel(const CommanderViewModel& other) =
    default;

}  // namespace commander
