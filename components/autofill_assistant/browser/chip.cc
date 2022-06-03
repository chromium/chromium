// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/chip.h"
#include "components/autofill_assistant/browser/user_action.h"

namespace autofill_assistant {

Chip::Chip() = default;
Chip::~Chip() = default;
Chip::Chip(const Chip& other) = default;
Chip::Chip(const ChipProto& proto)
    : type(proto.type()),
      icon(proto.icon()),
      text(proto.text()),
      sticky(proto.sticky()),
      content_description(proto.content_description()),
      is_content_description_set(proto.has_content_description()) {}

bool Chip::empty() const {
  return type == UNKNOWN_CHIP_TYPE && text.empty() && icon == NO_ICON;
}

void SetDefaultChipType(std::vector<UserAction>* user_actions) {
  for (UserAction& user_action : *user_actions) {
    if (user_action.chip().empty())
      continue;

    if (user_action.chip().type == UNKNOWN_CHIP_TYPE) {
      // Assume chips with unknown type are normal actions.
      user_action.chip().type = NORMAL_ACTION;
    }
  }
}

}  // namespace autofill_assistant
