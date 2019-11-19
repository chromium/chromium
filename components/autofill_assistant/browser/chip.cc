// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/chip.h"
#include "components/autofill_assistant/browser/user_action.h"

namespace autofill_assistant {

Chip::Chip() = default;
Chip::~Chip() = default;
Chip::Chip(const ChipProto& proto)
    : type(proto.type()),
      icon(proto.icon()),
      text(proto.text()),
      sticky(proto.sticky()) {}

bool Chip::empty() const {
  return type == UNKNOWN_CHIP_TYPE && text.empty() && icon == NO_ICON;
}

void SetDefaultChipType(std::vector<UserAction>* user_actions) {
  ChipType default_type = SUGGESTION;
  for (const UserAction& user_action : *user_actions) {
    if (user_action.chip().empty())
      continue;

    ChipType type = user_action.chip().type;
    if (type != UNKNOWN_CHIP_TYPE && type != SUGGESTION) {
      // If there's an action chip, assume chips with unknown type are also
      // actions.
      default_type = NORMAL_ACTION;
      break;
    }
  }
  for (UserAction& user_action : *user_actions) {
    if (user_action.chip().empty())
      continue;

    if (user_action.chip().type == UNKNOWN_CHIP_TYPE) {
      user_action.chip().type = default_type;
    }
  }
}

}  // namespace autofill_assistant
