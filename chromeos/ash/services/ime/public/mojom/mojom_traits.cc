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

// Returns the histogram with the given parameters.
// If any of the parameters are invalid, returns nullptr.
base::Histogram* GetHistogramStrict(
    const std::string& name,
    ash::ime::mojom::HistogramBucketType bucket_type,
    const base::Histogram::Sample minimum,
    const base::Histogram::Sample maximum,
    const size_t bucket_count) {
  // When `InspectConstructionArguments` receives certain invalid parameters, it
  // will attempt to adjust them to be correct and return true. So in order to
  // be strict about the validity of the parameters, ensure that the parameters
  // are not adjusted by `InspectConstructionArguments`.
  auto adjusted_minimum = minimum;
  auto adjusted_maximum = maximum;
  auto adjusted_bucket_count = bucket_count;
  if (!base::Histogram::InspectConstructionArguments(
          name, &adjusted_minimum, &adjusted_maximum, &adjusted_bucket_count)) {
    return nullptr;
  }
  if (minimum != adjusted_minimum || maximum != adjusted_maximum ||
      bucket_count != adjusted_bucket_count) {
    return nullptr;
  }

  // Returns nullptr if validation failed in `FactoryGet`.
  // This also relies on `Histogram::InspectConstructionArguments` called from
  // `FactoryGet` to validate the bucket parameters a second time:
  // https://source.chromium.org/chromium/chromium/src/+/main:base/metrics/histogram.cc;l=422
  switch (bucket_type) {
    case ash::ime::mojom::HistogramBucketType::kExponential:
      return static_cast<base::Histogram*>(base::Histogram::FactoryGet(
          name.data(), minimum, maximum, bucket_count,
          base::HistogramBase::kUmaTargetedHistogramFlag));
    case ash::ime::mojom::HistogramBucketType::kLinear:
      return static_cast<base::Histogram*>(base::LinearHistogram::FactoryGet(
          name.data(), minimum, maximum, bucket_count,
          base::HistogramBase::kUmaTargetedHistogramFlag));
    default:
      return nullptr;
  }
}

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

bool EnumTraits<SuggestionMode, AssistiveSuggestionMode>::FromMojom(
    SuggestionMode input,
    AssistiveSuggestionMode* output) {
  switch (input) {
    case SuggestionMode::kUnknown:
      // The browser process should never receive an unknown suggestion mode.
      // When adding a new SuggestionMode, the Chromium side should be updated
      // first to handle it, before changing the other calling side to send the
      // new suggestion mode.
      return false;
    case SuggestionMode::kCompletion:
      *output = AssistiveSuggestionMode::kCompletion;
      return true;
    case SuggestionMode::kPrediction:
      *output = AssistiveSuggestionMode::kPrediction;
      return true;
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

bool EnumTraits<SuggestionType, AssistiveSuggestionType>::FromMojom(
    SuggestionType input,
    AssistiveSuggestionType* output) {
  switch (input) {
    case SuggestionType::kUnknown:
      // The browser process should never receive an unknown suggestion type.
      // When adding a new SuggestionType, the Chromium side should be updated
      // first to handle it, before changing the other calling side to send the
      // new suggestion type.
      return false;
    case SuggestionType::kAssistivePersonalInfo:
      *output = AssistiveSuggestionType::kAssistivePersonalInfo;
      return true;
    case SuggestionType::kAssistiveEmoji:
      *output = AssistiveSuggestionType::kAssistiveEmoji;
      return true;
    case SuggestionType::kMultiWord:
      *output = AssistiveSuggestionType::kMultiWord;
      return true;
    case SuggestionType::kGrammar:
      *output = AssistiveSuggestionType::kGrammar;
      return true;
    case SuggestionType::kLongpressDiacritic:
      *output = AssistiveSuggestionType::kLongpressDiacritic;
      return true;
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

bool EnumTraits<AutocorrectSuggestionProviderMojo,
                AutocorrectSuggestionProvider>::
    FromMojom(AutocorrectSuggestionProviderMojo input,
              AutocorrectSuggestionProvider* output) {
  switch (input) {
    case AutocorrectSuggestionProviderMojo::kUsEnglishPrebundled:
      *output = AutocorrectSuggestionProvider::kUsEnglishPrebundled;
      return true;
    case AutocorrectSuggestionProviderMojo::kUsEnglishDownloaded:
      *output = AutocorrectSuggestionProvider::kUsEnglishDownloaded;
      return true;
    case AutocorrectSuggestionProviderMojo::kUsEnglish840:
      *output = AutocorrectSuggestionProvider::kUsEnglish840;
      return true;
    case AutocorrectSuggestionProviderMojo::kUsEnglish840V2:
      *output = AutocorrectSuggestionProvider::kUsEnglish840V2;
      return true;
    default:
      *output = AutocorrectSuggestionProvider::kUnknown;
      return true;
  }
}

bool StructTraits<
    ash::ime::mojom::BucketedHistogramDataView,
    base::Histogram*>::Read(ash::ime::mojom::BucketedHistogramDataView input,
                            base::Histogram** output) {
  std::string name;
  if (!input.ReadName(&name)) {
    return false;
  }
  if (!base::StartsWith(name, "Untrusted.")) {
    DLOG(ERROR) << "Invalid histogram name: " << name
                << ". BucketedHistogram name must begin with 'Untrusted.'.";
    return false;
  }

  const auto minimum =
      base::strict_cast<base::Histogram::Sample>(input.minimum());
  const auto maximum =
      base::strict_cast<base::Histogram::Sample>(input.maximum());
  const auto bucket_count = base::strict_cast<size_t>(input.bucket_count());

  base::Histogram* counter = GetHistogramStrict(name, input.bucket_type(),
                                                minimum, maximum, bucket_count);

  // `counter` will be nullptr if the parameters are invalid.
  if (counter == nullptr) {
    DLOG(ERROR) << "Invalid bucket parameters: bucket_type="
                << static_cast<int>(input.bucket_type())
                << ", minimum=" << minimum << ", maximum=" << maximum
                << ", bucket_count=" << bucket_count;
    return false;
  }

  // As a final defensive check, ensure that the constructed histogram has all
  // the expected parameters as passed in.
  if (!counter->HasConstructionArguments(minimum, maximum, bucket_count)) {
    DLOG(ERROR) << "Invalid histogram. Expected histogram with minimum="
                << minimum << ", maximum=" << maximum
                << ", bucket_count=" << bucket_count
                << ", but got minimum=" << counter->declared_min()
                << ", maximum=" << counter->declared_max()
                << ", bucket_count=" << counter->bucket_count();
    return false;
  }

  *output = counter;
  return true;
}

}  // namespace mojo
