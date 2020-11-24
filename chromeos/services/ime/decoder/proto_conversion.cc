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

InputFieldInfo::InputFieldType InputFieldTypeToProto(
    mojom::InputFieldType input_field_type) {
  switch (input_field_type) {
    case mojom::InputFieldType::kNoIME:
      return InputFieldInfo::INPUT_FIELD_TYPE_NO_IME;
    case mojom::InputFieldType::kText:
      return InputFieldInfo::INPUT_FIELD_TYPE_TEXT;
    case mojom::InputFieldType::kSearch:
      return InputFieldInfo::INPUT_FIELD_TYPE_SEARCH;
    case mojom::InputFieldType::kTelephone:
      return InputFieldInfo::INPUT_FIELD_TYPE_TELEPHONE;
    case mojom::InputFieldType::kURL:
      return InputFieldInfo::INPUT_FIELD_TYPE_URL;
    case mojom::InputFieldType::kEmail:
      return InputFieldInfo::INPUT_FIELD_TYPE_EMAIL;
    case mojom::InputFieldType::kNumber:
      return InputFieldInfo::INPUT_FIELD_TYPE_NUMBER;
    case mojom::InputFieldType::kPassword:
      return InputFieldInfo::INPUT_FIELD_TYPE_PASSWORD;
  }
}

InputFieldInfo::AutocorrectMode AutocorrectModeToProto(
    mojom::AutocorrectMode autocorrect_mode) {
  switch (autocorrect_mode) {
    case mojom::AutocorrectMode::kDisabled:
      return InputFieldInfo::AUTOCORRECT_MODE_DISABLED;
    case mojom::AutocorrectMode::kEnabled:
      return InputFieldInfo::AUTOCORRECT_MODE_ENABLED;
  }
}

InputFieldInfo::PersonalizationMode PersonalizationModeToProto(
    mojom::PersonalizationMode personalization_mode) {
  switch (personalization_mode) {
    case mojom::PersonalizationMode::kDisabled:
      return InputFieldInfo::PERSONALIZATION_MODE_DISABLED;
    case mojom::PersonalizationMode::kEnabled:
      return InputFieldInfo::PERSONALIZATION_MODE_ENABLED;
  }
}

}  // namespace

ime::PublicMessage OnInputMethodChangedToProto(uint64_t seq_id,
                                               const std::string& engine_id) {
  ime::PublicMessage message;
  message.set_seq_id(seq_id);

  message.mutable_on_input_method_changed()->set_engine_id(engine_id);
  return message;
}

ime::PublicMessage OnFocusToProto(uint64_t seq_id,
                                  mojom::InputFieldInfoPtr input_field_info) {
  ime::PublicMessage message;
  message.set_seq_id(seq_id);

  ime::InputFieldInfo& proto_info = *message.mutable_on_focus()->mutable_info();
  proto_info.set_type(InputFieldTypeToProto(input_field_info->type));
  proto_info.set_autocorrect(
      AutocorrectModeToProto(input_field_info->autocorrect));
  proto_info.set_personalization(
      PersonalizationModeToProto(input_field_info->personalization));
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

ime::PublicMessage OnCompositionCanceledToProto(uint64_t seq_id) {
  ime::PublicMessage message;
  message.set_seq_id(seq_id);

  *message.mutable_on_composition_canceled() = ime::OnCompositionCanceled();

  return message;
}

mojom::AutocorrectSpanPtr ProtoToAutocorrectSpan(
    const chromeos::ime::AutocorrectSpan& autocorrect_span) {
  auto mojo_autocorrect_span = mojom::AutocorrectSpan::New();
  mojo_autocorrect_span->autocorrect_range =
      gfx::Range(autocorrect_span.autocorrect_range().start(),
                 autocorrect_span.autocorrect_range().end());
  mojo_autocorrect_span->original_text = autocorrect_span.original_text();
  return mojo_autocorrect_span;
}

}  // namespace ime
}  // namespace chromeos
