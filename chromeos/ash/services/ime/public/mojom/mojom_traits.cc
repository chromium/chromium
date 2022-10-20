// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/public/mojom/mojom_traits.h"

#include "chromeos/ash/services/ime/public/mojom/input_method_host.mojom-shared.h"

namespace mojo {
namespace {

using AssistiveWindow = ash::ime::AssistiveWindow;
using AssistiveWindowDataView = ash::ime::mojom::AssistiveWindowDataView;
using AssistiveWindowType = ash::ime::AssistiveWindowType;
using AssistiveWindowTypeMojo = ash::ime::mojom::AssistiveWindowType;
using CompletionCandidateDataView =
    ash::ime::mojom::CompletionCandidateDataView;
using SuggestionMode = ash::ime::mojom::SuggestionMode;
using SuggestionType = ash::ime::mojom::SuggestionType;
using SuggestionCandidateDataView =
    ash::ime::mojom::SuggestionCandidateDataView;
using TextCompletionCandidate = ash::ime::TextCompletionCandidate;
using TextSuggestionMode = ash::ime::TextSuggestionMode;
using TextSuggestionType = ash::ime::TextSuggestionType;
using TextSuggestion = ash::ime::TextSuggestion;

}  // namespace

SuggestionMode EnumTraits<SuggestionMode, TextSuggestionMode>::ToMojom(
    TextSuggestionMode mode) {
  switch (mode) {
    case TextSuggestionMode::kCompletion:
      return SuggestionMode::kCompletion;
    case TextSuggestionMode::kPrediction:
      return SuggestionMode::kPrediction;
  }
}

bool EnumTraits<SuggestionMode, TextSuggestionMode>::FromMojom(
    SuggestionMode input,
    TextSuggestionMode* output) {
  switch (input) {
    case SuggestionMode::kUnknown:
      // The browser process should never receive an unknown suggestion mode.
      // When adding a new SuggestionMode, the Chromium side should be updated
      // first to handle it, before changing the other calling side to send the
      // new suggestion mode.
      return false;
    case SuggestionMode::kCompletion:
      *output = TextSuggestionMode::kCompletion;
      return true;
    case SuggestionMode::kPrediction:
      *output = TextSuggestionMode::kPrediction;
      return true;
  }
}

SuggestionType EnumTraits<SuggestionType, TextSuggestionType>::ToMojom(
    TextSuggestionType type) {
  switch (type) {
    case TextSuggestionType::kAssistivePersonalInfo:
      return SuggestionType::kAssistivePersonalInfo;
    case TextSuggestionType::kAssistiveEmoji:
      return SuggestionType::kAssistiveEmoji;
    case TextSuggestionType::kMultiWord:
      return SuggestionType::kMultiWord;
    case TextSuggestionType::kGrammar:
      return SuggestionType::kGrammar;
    case TextSuggestionType::kLongpressDiacritic:
      return SuggestionType::kLongpressDiacritic;
  }
}

bool EnumTraits<SuggestionType, TextSuggestionType>::FromMojom(
    SuggestionType input,
    TextSuggestionType* output) {
  switch (input) {
    case SuggestionType::kUnknown:
      // The browser process should never receive an unknown suggestion type.
      // When adding a new SuggestionType, the Chromium side should be updated
      // first to handle it, before changing the other calling side to send the
      // new suggestion type.
      return false;
    case SuggestionType::kAssistivePersonalInfo:
      *output = TextSuggestionType::kAssistivePersonalInfo;
      return true;
    case SuggestionType::kAssistiveEmoji:
      *output = TextSuggestionType::kAssistiveEmoji;
      return true;
    case SuggestionType::kMultiWord:
      *output = TextSuggestionType::kMultiWord;
      return true;
    case SuggestionType::kGrammar:
      *output = TextSuggestionType::kGrammar;
      return true;
    case SuggestionType::kLongpressDiacritic:
      *output = TextSuggestionType::kLongpressDiacritic;
      return true;
  }
}

bool StructTraits<SuggestionCandidateDataView, TextSuggestion>::Read(
    SuggestionCandidateDataView input,
    TextSuggestion* output) {
  if (!input.ReadMode(&output->mode))
    return false;
  if (!input.ReadType(&output->type))
    return false;
  if (!input.ReadText(&output->text))
    return false;
  output->confirmed_length = input.confirmed_length();
  return true;
}

bool StructTraits<CompletionCandidateDataView, TextCompletionCandidate>::Read(
    CompletionCandidateDataView input,
    TextCompletionCandidate* output) {
  if (!input.ReadText(&output->text))
    return false;
  output->score = input.normalized_score();
  return true;
}

AssistiveWindowTypeMojo
EnumTraits<AssistiveWindowTypeMojo, AssistiveWindowType>::ToMojom(
    AssistiveWindowType type) {
  switch (type) {
    case AssistiveWindowType::kUndoWindow:
      return AssistiveWindowTypeMojo::kUndo;
    case AssistiveWindowType::kEmojiSuggestion:
      return AssistiveWindowTypeMojo::kEmojiSuggestion;
    case AssistiveWindowType::kPersonalInfoSuggestion:
      return AssistiveWindowTypeMojo::kPersonalInfoSuggestion;
    case AssistiveWindowType::kGrammarSuggestion:
      return AssistiveWindowTypeMojo::kGrammarSuggestion;
    case AssistiveWindowType::kMultiWordSuggestion:
      return AssistiveWindowTypeMojo::kMultiWordSuggestion;
    case AssistiveWindowType::kLongpressDiacriticsSuggestion:
      return AssistiveWindowTypeMojo::kLongpressDiacriticsSuggestion;
    case AssistiveWindowType::kNone:
    default:
      return AssistiveWindowTypeMojo::kHidden;
  }
}

bool EnumTraits<AssistiveWindowTypeMojo, AssistiveWindowType>::FromMojom(
    AssistiveWindowTypeMojo input,
    AssistiveWindowType* output) {
  switch (input) {
    case AssistiveWindowTypeMojo::kHidden:
      *output = AssistiveWindowType::kNone;
      return true;
    case AssistiveWindowTypeMojo::kUndo:
      *output = AssistiveWindowType::kUndoWindow;
      return true;
    case AssistiveWindowTypeMojo::kEmojiSuggestion:
      *output = AssistiveWindowType::kEmojiSuggestion;
      return true;
    case AssistiveWindowTypeMojo::kPersonalInfoSuggestion:
      *output = AssistiveWindowType::kPersonalInfoSuggestion;
      return true;
    case AssistiveWindowTypeMojo::kGrammarSuggestion:
      *output = AssistiveWindowType::kGrammarSuggestion;
      return true;
    case AssistiveWindowTypeMojo::kMultiWordSuggestion:
      *output = AssistiveWindowType::kMultiWordSuggestion;
      return true;
    case AssistiveWindowTypeMojo::kLongpressDiacriticsSuggestion:
      *output = AssistiveWindowType::kLongpressDiacriticsSuggestion;
      return true;
  }
}

bool StructTraits<AssistiveWindowDataView, AssistiveWindow>::Read(
    AssistiveWindowDataView input,
    AssistiveWindow* output) {
  if (!input.ReadType(&output->type))
    return false;
  if (!input.ReadCandidates(&output->candidates))
    return false;
  return true;
}

}  // namespace mojo
