// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/decoder/proto_conversion.h"

namespace chromeos {
namespace ime {
namespace {

ModifierState ModifierStateToProto(mojom::ModifierStatePtr modifier_state) {
  ModifierState result;
  result.set_alt(modifier_state->alt);
  result.set_alt_graph(modifier_state->alt_graph);
  result.set_caps_lock(modifier_state->caps_lock);
  result.set_control(modifier_state->control);
  result.set_meta(modifier_state->meta);
  result.set_shift(modifier_state->shift);
  return result;
}

}  // namespace

ime::PublicMessage OnFocusToProto(uint64_t seq_id) {
  ime::PublicMessage message;
  message.set_seq_id(seq_id);

  *message.mutable_on_focus() = ime::OnFocus();
  return message;
}

ime::PublicMessage OnBlurToProto(uint64_t seq_id) {
  ime::PublicMessage message;
  message.set_seq_id(seq_id);

  *message.mutable_on_blur() = ime::OnBlur();
  return message;
}

ime::PublicMessage OnKeyEventToProto(uint64_t seq_id,
                                     mojom::PhysicalKeyEventPtr event) {
  ime::PublicMessage message;
  message.set_seq_id(seq_id);

  ime::PhysicalKeyEvent& key_event =
      *message.mutable_on_key_event()->mutable_key_event();
  key_event.set_type(event->type == mojom::KeyEventType::kKeyDown
                         ? ime::PhysicalKeyEvent::EVENT_TYPE_KEY_DOWN
                         : ime::PhysicalKeyEvent::EVENT_TYPE_KEY_UP);
  key_event.set_code(event->code);
  key_event.set_key(event->key);
  *key_event.mutable_modifier_state() =
      ModifierStateToProto(std::move(event->modifier_state));
  return message;
}

ime::PublicMessage OnSurroundingTextChangedToProto(
    uint64_t seq_id,
    const std::string& text,
    uint32_t offset,
    mojom::SelectionRangePtr selection_range) {
  ime::PublicMessage message;
  message.set_seq_id(seq_id);

  ime::OnSurroundingTextChanged& params =
      *message.mutable_on_surrounding_text_changed();
  params.set_text(text);
  params.set_offset(offset);
  params.mutable_selection_range()->set_anchor(selection_range->anchor);
  params.mutable_selection_range()->set_focus(selection_range->focus);

  return message;
}

}  // namespace ime
}  // namespace chromeos
