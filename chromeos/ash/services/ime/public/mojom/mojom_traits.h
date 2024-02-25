// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_IME_PUBLIC_MOJOM_MOJOM_TRAITS_H_
#define CHROMEOS_ASH_SERVICES_IME_PUBLIC_MOJOM_MOJOM_TRAITS_H_

#include <string_view>

#include "base/metrics/histogram.h"
#include "chromeos/ash/services/ime/public/cpp/assistive_suggestions.h"
#include "chromeos/ash/services/ime/public/cpp/autocorrect.h"
#include "chromeos/ash/services/ime/public/mojom/input_method_host.mojom-shared.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "ui/gfx/range/range.h"

namespace mojo {

template <>
struct EnumTraits<ash::ime::mojom::SuggestionMode,
                  ash::ime::AssistiveSuggestionMode> {
  using AssistiveSuggestionMode = ::ash::ime::AssistiveSuggestionMode;
  using SuggestionMode = ::ash::ime::mojom::SuggestionMode;

  static SuggestionMode ToMojom(AssistiveSuggestionMode mode);
  static bool FromMojom(SuggestionMode input, AssistiveSuggestionMode* output);
};

template <>
struct EnumTraits<ash::ime::mojom::SuggestionType,
                  ash::ime::AssistiveSuggestionType> {
  using AssistiveSuggestionType = ::ash::ime::AssistiveSuggestionType;
  using SuggestionType = ::ash::ime::mojom::SuggestionType;

  static SuggestionType ToMojom(AssistiveSuggestionType type);
  static bool FromMojom(SuggestionType input, AssistiveSuggestionType* output);
};

template <>
struct StructTraits<ash::ime::mojom::SuggestionCandidateDataView,
                    ash::ime::AssistiveSuggestion> {
  using SuggestionCandidateDataView =
      ::ash::ime::mojom::SuggestionCandidateDataView;
  using AssistiveSuggestion = ::ash::ime::AssistiveSuggestion;
  using AssistiveSuggestionMode = ::ash::ime::AssistiveSuggestionMode;
  using AssistiveSuggestionType = ::ash::ime::AssistiveSuggestionType;

  static AssistiveSuggestionMode mode(const AssistiveSuggestion& suggestion) {
    return suggestion.mode;
  }
  static AssistiveSuggestionType type(const AssistiveSuggestion& suggestion) {
    return suggestion.type;
  }
  static const std::string& text(const AssistiveSuggestion& suggestion) {
    return suggestion.text;
  }
  static size_t confirmed_length(const AssistiveSuggestion& suggestion) {
    return suggestion.confirmed_length;
  }

  static bool Read(SuggestionCandidateDataView input,
                   AssistiveSuggestion* output);
};

template <>
struct StructTraits<ash::ime::mojom::SuggestionsTextContextDataView,
                    ash::ime::SuggestionsTextContext> {
  using SuggestionsTextContextDataView =
      ::ash::ime::mojom::SuggestionsTextContextDataView;
  using SuggestionsTextContext = ::ash::ime::SuggestionsTextContext;

  static const std::string& last_n_chars(
      const SuggestionsTextContext& context) {
    return context.last_n_chars;
  }

  static size_t surrounding_text_length(const SuggestionsTextContext& context) {
    return context.surrounding_text_length;
  }

  static bool Read(SuggestionsTextContextDataView input,
                   SuggestionsTextContext* output);
};

template <>
struct StructTraits<ash::ime::mojom::CompletionCandidateDataView,
                    ash::ime::DecoderCompletionCandidate> {
  using CompletionCandidateDataView =
      ::ash::ime::mojom::CompletionCandidateDataView;
  using DecoderCompletionCandidate = ::ash::ime::DecoderCompletionCandidate;

  static const std::string& text(const DecoderCompletionCandidate& candidate) {
    return candidate.text;
  }

  static const float& normalized_score(
      const DecoderCompletionCandidate& candidate) {
    return candidate.score;
  }

  static bool Read(CompletionCandidateDataView input,
                   DecoderCompletionCandidate* output);
};

template <>
struct StructTraits<ash::ime::mojom::TextRangeDataView, gfx::Range> {
  static uint32_t start(const gfx::Range& r) { return r.start(); }
  static uint32_t end(const gfx::Range& r) { return r.end(); }
  static bool Read(ash::ime::mojom::TextRangeDataView data, gfx::Range* out) {
    out->set_start(data.start());
    out->set_end(data.end());
    return true;
  }
};

template <>
struct EnumTraits<ash::ime::mojom::AssistiveWindowType,
                  ash::ime::AssistiveWindowType> {
  using AssistiveWindowTypeMojo = ::ash::ime::mojom::AssistiveWindowType;
  using AssistiveWindowType = ::ash::ime::AssistiveWindowType;

  static AssistiveWindowTypeMojo ToMojom(AssistiveWindowType type);
  static bool FromMojom(AssistiveWindowTypeMojo input,
                        AssistiveWindowType* output);
};

template <>
struct StructTraits<ash::ime::mojom::AssistiveWindowDataView,
                    ash::ime::AssistiveWindow> {
  using AssistiveWindowDataView = ::ash::ime::mojom::AssistiveWindowDataView;
  using AssistiveWindowType = ::ash::ime::AssistiveWindowType;
  using AssistiveWindow = ::ash::ime::AssistiveWindow;
  using AssistiveSuggestion = ::ash::ime::AssistiveSuggestion;

  static AssistiveWindowType type(const AssistiveWindow& window) {
    return window.type;
  }
  static const std::vector<AssistiveSuggestion>& candidates(
      const AssistiveWindow& window) {
    return window.candidates;
  }

  static bool Read(AssistiveWindowDataView input, AssistiveWindow* output);
};

template <>
struct EnumTraits<ash::ime::mojom::AutocorrectSuggestionProvider,
                  ash::ime::AutocorrectSuggestionProvider> {
  using AutocorrectSuggestionProviderMojo =
      ::ash::ime::mojom::AutocorrectSuggestionProvider;
  using AutocorrectSuggestionProvider =
      ::ash::ime::AutocorrectSuggestionProvider;

  static AutocorrectSuggestionProviderMojo ToMojom(
      AutocorrectSuggestionProvider provider);
  static bool FromMojom(AutocorrectSuggestionProviderMojo input,
                        AutocorrectSuggestionProvider* output);
};

template <>
struct StructTraits<ash::ime::mojom::BucketedHistogramDataView,
                    base::Histogram*> {
  static bool IsNull(base::Histogram* histogram) { return !histogram; }

  static void SetToNull(base::Histogram** histogram) { *histogram = nullptr; }

  static std::string_view name(base::Histogram* histogram) {
    return histogram->histogram_name();
  }

  static ash::ime::mojom::HistogramBucketType bucket_type(
      base::Histogram* histogram) {
    switch (histogram->GetHistogramType()) {
      case base::HISTOGRAM:
        return ash::ime::mojom::HistogramBucketType::kExponential;
      case base::LINEAR_HISTOGRAM:
      case base::BOOLEAN_HISTOGRAM:
        return ash::ime::mojom::HistogramBucketType::kLinear;
      case base::CUSTOM_HISTOGRAM:
      case base::SPARSE_HISTOGRAM:
      case base::DUMMY_HISTOGRAM:
        CHECK(false) << "Invalid histogram bucket type: "
                     << static_cast<int>(histogram->GetHistogramType());
        return ash::ime::mojom::HistogramBucketType::kExponential;
    }
  }

  static uint16_t minimum(base::Histogram* histogram) {
    return base::checked_cast<uint16_t>(histogram->declared_min());
  }

  static uint16_t maximum(base::Histogram* histogram) {
    return base::checked_cast<uint16_t>(histogram->declared_max());
  }

  static uint16_t bucket_count(base::Histogram* histogram) {
    return base::checked_cast<uint16_t>(histogram->bucket_count());
  }

  static bool Read(ash::ime::mojom::BucketedHistogramDataView input,
                   base::Histogram** output);
};

}  // namespace mojo

#endif  // CHROMEOS_ASH_SERVICES_IME_PUBLIC_MOJOM_MOJOM_TRAITS_H_
