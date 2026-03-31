// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/public/mojom/ime_mojom_traits.h"

#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "chromeos/ash/services/ime/public/mojom/input_method_host.mojom-shared.h"

namespace mojo {
namespace {

using AssistiveWindow = ash::ime::AssistiveWindow;
using AssistiveWindowDataView = ash::ime::mojom::AssistiveWindowDataView;
using AssistiveWindowType = ash::ime::AssistiveWindowType;
using AssistiveWindowTypeMojo = ash::ime::mojom::AssistiveWindowType;
using CompletionCandidateDataView =
    ash::ime::mojom::CompletionCandidateDataView;
using AssistiveSuggestionMode = ash::ime::AssistiveSuggestionMode;
using SuggestionMode = ash::ime::mojom::SuggestionMode;
using SuggestionType = ash::ime::mojom::SuggestionType;
using SuggestionsTextContextDataView =
    ash::ime::mojom::SuggestionsTextContextDataView;
using SuggestionsTextContext = ash::ime::SuggestionsTextContext;
using SuggestionCandidateDataView =
    ash::ime::mojom::SuggestionCandidateDataView;
using DecoderCompletionCandidate = ash::ime::DecoderCompletionCandidate;
using AssistiveSuggestionType = ash::ime::AssistiveSuggestionType;
using AssistiveSuggestion = ash::ime::AssistiveSuggestion;
using AutocorrectSuggestionProvider = ash::ime::AutocorrectSuggestionProvider;
using AutocorrectSuggestionProviderMojo =
    ash::ime::mojom::AutocorrectSuggestionProvider;

}  // namespace

SuggestionMode EnumTraits<SuggestionMode, AssistiveSuggestionMode>::ToMojom(
    AssistiveSuggestionMode mode) {
  switch (mode) {
    case AssistiveSuggestionMode::kCompletion:
      return SuggestionMode::kCompletion;
    case AssistiveSuggestionMode::kPrediction:
      return SuggestionMode::kPrediction;
  }
}

AssistiveSuggestionMode
EnumTraits<SuggestionMode, AssistiveSuggestionMode>::FromMojom(
    SuggestionMode input) {
  switch (input) {
    case SuggestionMode::kUnknown:
      // The browser process should never receive an unknown suggestion mode.
      // When adding a new SuggestionMode, the Chromium side should be updated
      // first to handle it, before changing the other calling side to send the
      // new suggestion mode.
      NOTREACHED();
    case SuggestionMode::kCompletion:
      return AssistiveSuggestionMode::kCompletion;
    case SuggestionMode::kPrediction:
      return AssistiveSuggestionMode::kPrediction;
  }
}

SuggestionType EnumTraits<SuggestionType, AssistiveSuggestionType>::ToMojom(
    AssistiveSuggestionType type) {
  switch (type) {
    case AssistiveSuggestionType::kAssistivePersonalInfo:
      return SuggestionType::kAssistivePersonalInfo;
    case AssistiveSuggestionType::kAssistiveEmoji:
      return SuggestionType::kAssistiveEmoji;
    case AssistiveSuggestionType::kMultiWord:
      return SuggestionType::kMultiWord;
    case AssistiveSuggestionType::kGrammar:
      return SuggestionType::kGrammar;
    case AssistiveSuggestionType::kLongpressDiacritic:
      return SuggestionType::kLongpressDiacritic;
  }
}

AssistiveSuggestionType
EnumTraits<SuggestionType, AssistiveSuggestionType>::FromMojom(
    SuggestionType input) {
  switch (input) {
    case SuggestionType::kUnknown:
      // The browser process should never receive an unknown suggestion type.
      // When adding a new SuggestionType, the Chromium side should be updated
      // first to handle it, before changing the other calling side to send the
      // new suggestion type.
      NOTREACHED();
    case SuggestionType::kAssistivePersonalInfo:
      return AssistiveSuggestionType::kAssistivePersonalInfo;
    case SuggestionType::kAssistiveEmoji:
      return AssistiveSuggestionType::kAssistiveEmoji;
    case SuggestionType::kMultiWord:
      return AssistiveSuggestionType::kMultiWord;
    case SuggestionType::kGrammar:
      return AssistiveSuggestionType::kGrammar;
    case SuggestionType::kLongpressDiacritic:
      return AssistiveSuggestionType::kLongpressDiacritic;
  }
}

bool StructTraits<SuggestionCandidateDataView, AssistiveSuggestion>::Read(
    SuggestionCandidateDataView input,
    AssistiveSuggestion* output) {
  if (!input.ReadMode(&output->mode)) {
    return false;
  }
  if (!input.ReadType(&output->type)) {
    return false;
  }
  if (!input.ReadText(&output->text)) {
    return false;
  }
  output->confirmed_length = input.confirmed_length();
  return true;
}

bool StructTraits<SuggestionsTextContextDataView, SuggestionsTextContext>::Read(
    SuggestionsTextContextDataView input,
    SuggestionsTextContext* output) {
  if (!input.ReadLastNChars(&output->last_n_chars)) {
    return false;
  }
  output->surrounding_text_length = input.surrounding_text_length();
  return true;
}

bool StructTraits<CompletionCandidateDataView, DecoderCompletionCandidate>::
    Read(CompletionCandidateDataView input,
         DecoderCompletionCandidate* output) {
  if (!input.ReadText(&output->text)) {
    return false;
  }
  output->score = input.normalized_score();
  return true;
}

AssistiveWindowTypeMojo
EnumTraits<AssistiveWindowTypeMojo, AssistiveWindowType>::ToMojom(
    AssistiveWindowType type) {
  switch (type) {
    case AssistiveWindowType::kUndoWindow:
      return AssistiveWindowTypeMojo::kUndo;
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

AssistiveWindowType
EnumTraits<AssistiveWindowTypeMojo, AssistiveWindowType>::FromMojom(
    AssistiveWindowTypeMojo input) {
  switch (input) {
    case AssistiveWindowTypeMojo::kHidden:
      return AssistiveWindowType::kNone;
    case AssistiveWindowTypeMojo::kUndo:
      return AssistiveWindowType::kUndoWindow;
    case AssistiveWindowTypeMojo::kPersonalInfoSuggestion:
      return AssistiveWindowType::kPersonalInfoSuggestion;
    case AssistiveWindowTypeMojo::kGrammarSuggestion:
      return AssistiveWindowType::kGrammarSuggestion;
    case AssistiveWindowTypeMojo::kMultiWordSuggestion:
      return AssistiveWindowType::kMultiWordSuggestion;
    case AssistiveWindowTypeMojo::kLongpressDiacriticsSuggestion:
      return AssistiveWindowType::kLongpressDiacriticsSuggestion;
    default:
      NOTREACHED();
  }
}

bool StructTraits<AssistiveWindowDataView, AssistiveWindow>::Read(
    AssistiveWindowDataView input,
    AssistiveWindow* output) {
  if (!input.ReadType(&output->type)) {
    return false;
  }
  if (!input.ReadCandidates(&output->candidates)) {
    return false;
  }
  return true;
}

AutocorrectSuggestionProviderMojo
EnumTraits<AutocorrectSuggestionProviderMojo, AutocorrectSuggestionProvider>::
    ToMojom(AutocorrectSuggestionProvider provider) {
  switch (provider) {
    case AutocorrectSuggestionProvider::kUsEnglishPrebundled:
      return AutocorrectSuggestionProviderMojo::kUsEnglishPrebundled;
    case AutocorrectSuggestionProvider::kUsEnglishDownloaded:
      return AutocorrectSuggestionProviderMojo::kUsEnglishDownloaded;
    case AutocorrectSuggestionProvider::kUsEnglish840:
      return AutocorrectSuggestionProviderMojo::kUsEnglish840;
    case AutocorrectSuggestionProvider::kUsEnglish840V2:
      return AutocorrectSuggestionProviderMojo::kUsEnglish840V2;
    default:
      return AutocorrectSuggestionProviderMojo::kUnknown;
  }
}

AutocorrectSuggestionProvider
EnumTraits<AutocorrectSuggestionProviderMojo, AutocorrectSuggestionProvider>::
    FromMojom(AutocorrectSuggestionProviderMojo input) {
  switch (input) {
    case AutocorrectSuggestionProviderMojo::kUsEnglishPrebundled:
      return AutocorrectSuggestionProvider::kUsEnglishPrebundled;
    case AutocorrectSuggestionProviderMojo::kUsEnglishDownloaded:
      return AutocorrectSuggestionProvider::kUsEnglishDownloaded;
    case AutocorrectSuggestionProviderMojo::kUsEnglish840:
      return AutocorrectSuggestionProvider::kUsEnglish840;
    case AutocorrectSuggestionProviderMojo::kUsEnglish840V2:
      return AutocorrectSuggestionProvider::kUsEnglish840V2;
    default:
      return AutocorrectSuggestionProvider::kUnknown;
  }
}

}  // namespace mojo
