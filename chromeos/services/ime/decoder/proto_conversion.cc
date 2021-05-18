// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/decoder/proto_conversion.h"

#include "chromeos/services/ime/public/cpp/suggestions.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

SuggestionMode TextSuggestionModeToProto(TextSuggestionMode mode) {
  switch (mode) {
    case TextSuggestionMode::kPrediction:
      return SuggestionMode::SUGGESTION_MODE_PREDICTION;
    case TextSuggestionMode::kCompletion:
      return SuggestionMode::SUGGESTION_MODE_COMPLETION;
  }
}

SuggestionType TextSuggestionTypeToProto(TextSuggestionType type) {
  switch (type) {
    case TextSuggestionType::kAssistiveEmoji:
      return SuggestionType::SUGGESTION_TYPE_ASSISTIVE_EMOJI;
    case TextSuggestionType::kAssistivePersonalInfo:
      return SuggestionType::SUGGESTION_TYPE_ASSISTIVE_PERSONAL_INFO;
    case TextSuggestionType::kMultiWord:
      return SuggestionType::SUGGESTION_TYPE_MULTI_WORD;
  }
}

TextSuggestionMode ProtoToTextSuggestionMode(
    const SuggestionMode& suggestion_mode) {
  switch (suggestion_mode) {
    case SuggestionMode::SUGGESTION_MODE_PREDICTION:
      return TextSuggestionMode::kPrediction;
    case SuggestionMode::SUGGESTION_MODE_COMPLETION:
      return TextSuggestionMode::kCompletion;
    default:
      return TextSuggestionMode::kPrediction;
  }
}

absl::optional<TextSuggestionType> ProtoToTextSuggestionType(
    const SuggestionType& suggestion_type) {
  switch (suggestion_type) {
    case SuggestionType::SUGGESTION_TYPE_ASSISTIVE_EMOJI:
      return TextSuggestionType::kAssistiveEmoji;
    case SuggestionType::SUGGESTION_TYPE_ASSISTIVE_PERSONAL_INFO:
      return TextSuggestionType::kAssistivePersonalInfo;
    case SuggestionType::SUGGESTION_TYPE_MULTI_WORD:
      return TextSuggestionType::kMultiWord;
    default:
      return absl::nullopt;
  }
}

absl::optional<mojom::InputMethodApiOperation> InputMethodApiOperationToMojo(
    NonCompliantApiMetric::InputMethodApiOperation operation) {
  switch (operation) {
    case NonCompliantApiMetric::OPERATION_UNSPECIFIED:
      return absl::nullopt;
    case NonCompliantApiMetric::OPERATION_COMMIT_TEXT:
      return mojom::InputMethodApiOperation::kCommitText;
    case NonCompliantApiMetric::OPERATION_SET_COMPOSITION_TEXT:
      return mojom::InputMethodApiOperation::kSetCompositionText;
    case NonCompliantApiMetric::OPERATION_DELETE_SURROUNDING_TEXT:
      return mojom::InputMethodApiOperation::kDeleteSurroundingText;
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

ime::PublicMessage SuggestionsResponseToProto(
    uint64_t seq_id,
    mojom::SuggestionsResponsePtr response) {
  ime::PublicMessage message;
  message.set_seq_id(seq_id);
  ime::SuggestionsResponse* suggestions_response =
      message.mutable_suggestions_response();

  for (const auto& text_suggestion : response->candidates) {
    ime::SuggestionCandidate* candidate =
        suggestions_response->add_candidates();
    candidate->set_mode(TextSuggestionModeToProto(text_suggestion.mode));
    candidate->set_type(TextSuggestionTypeToProto(text_suggestion.type));
    candidate->set_text(text_suggestion.text);
  }

  return message;
}

mojom::AutocorrectSpanPtr ProtoToAutocorrectSpan(
    const chromeos::ime::AutocorrectSpan& autocorrect_span) {
  auto mojo_autocorrect_span = mojom::AutocorrectSpan::New();
  mojo_autocorrect_span->autocorrect_range =
      gfx::Range(autocorrect_span.autocorrect_range().start(),
                 autocorrect_span.autocorrect_range().end());
  mojo_autocorrect_span->original_text = autocorrect_span.original_text();
  mojo_autocorrect_span->current_text = autocorrect_span.current_text();
  return mojo_autocorrect_span;
}

mojom::SuggestionsRequestPtr ProtoToSuggestionsRequest(
    const chromeos::ime::SuggestionsRequest& suggestions_request) {
  auto mojo_suggestions_request = mojom::SuggestionsRequest::New();
  mojo_suggestions_request->text = suggestions_request.text();
  mojo_suggestions_request->mode =
      ProtoToTextSuggestionMode(suggestions_request.suggestion_mode());
  for (const auto& candidate : suggestions_request.completion_candidates()) {
    mojo_suggestions_request->completion_candidates.push_back(
        TextCompletionCandidate{.text = candidate.text(),
                                .score = candidate.normalized_score()});
  }
  return mojo_suggestions_request;
}

std::vector<TextSuggestion> ProtoToTextSuggestions(
    const chromeos::ime::DisplaySuggestions& display_suggestions) {
  std::vector<TextSuggestion> suggestions;
  for (const auto& candidate : display_suggestions.candidates()) {
    absl::optional<TextSuggestionType> suggestion_type =
        ProtoToTextSuggestionType(candidate.type());
    if (suggestion_type) {
      // Drop any unexpected suggestion types
      suggestions.push_back(
          TextSuggestion{.mode = ProtoToTextSuggestionMode(candidate.mode()),
                         .type = suggestion_type.value(),
                         .text = candidate.text()});
    }
  }
  return suggestions;
}

mojom::UkmEntryPtr ProtoToUkmEntry(const RecordUkm& record_ukm) {
  switch (record_ukm.entry_case()) {
    case RecordUkm::ENTRY_NOT_SET:
      return nullptr;
    case RecordUkm::kNonCompliantApi: {
      auto operation = InputMethodApiOperationToMojo(
          record_ukm.non_compliant_api().non_compliant_operation());
      if (!operation)
        return nullptr;
      return mojom::UkmEntry::NewNonCompliantApi(
          mojom::NonCompliantApiMetric::New(*operation));
    }
  }
}

}  // namespace ime
}  // namespace chromeos
