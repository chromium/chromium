// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commander/command_source.h"

namespace commander {

CommandItem::CommandItem() = default;
CommandItem::CommandItem(const std::u16string& title,
                         double score,
                         const std::vector<gfx::Range>& ranges)
    : title(title), score(score), matched_ranges(ranges) {}
CommandItem::~CommandItem() = default;
CommandItem::CommandItem(CommandItem&& other) = default;
CommandItem& CommandItem::operator=(CommandItem&& other) = default;

CommandItem::Type CommandItem::GetType() {
  if (absl::get_if<CompositeCommand>(&command))
    return kComposite;
  return kOneShot;
}

}  // namespace commander
