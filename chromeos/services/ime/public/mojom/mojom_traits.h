// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_IME_PUBLIC_MOJOM_MOJOM_TRAITS_H_
#define CHROMEOS_SERVICES_IME_PUBLIC_MOJOM_MOJOM_TRAITS_H_

#include "chromeos/services/ime/public/cpp/suggestions.h"
#include "chromeos/services/ime/public/mojom/input_engine.mojom-shared.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct EnumTraits<chromeos::ime::mojom::SuggestionMode,
                  chromeos::ime::TextSuggestionMode> {
  using TextSuggestionMode = ::chromeos::ime::TextSuggestionMode;
  using SuggestionMode = ::chromeos::ime::mojom::SuggestionMode;

  static SuggestionMode ToMojom(TextSuggestionMode mode);
  static bool FromMojom(SuggestionMode input, TextSuggestionMode* output);
};

template <>
struct EnumTraits<chromeos::ime::mojom::SuggestionType,
                  chromeos::ime::TextSuggestionType> {
  using TextSuggestionType = ::chromeos::ime::TextSuggestionType;
  using SuggestionType = ::chromeos::ime::mojom::SuggestionType;

  static SuggestionType ToMojom(TextSuggestionType type);
  static bool FromMojom(SuggestionType input, TextSuggestionType* output);
};

template <>
struct StructTraits<chromeos::ime::mojom::SuggestionCandidateDataView,
                    chromeos::ime::TextSuggestion> {
  using SuggestionCandidateDataView =
      ::chromeos::ime::mojom::SuggestionCandidateDataView;
  using TextSuggestion = ::chromeos::ime::TextSuggestion;
  using TextSuggestionMode = ::chromeos::ime::TextSuggestionMode;
  using TextSuggestionType = ::chromeos::ime::TextSuggestionType;

  static TextSuggestionMode mode(const TextSuggestion& suggestion) {
    return suggestion.mode;
  }
  static TextSuggestionType type(const TextSuggestion& suggestion) {
    return suggestion.type;
  }
  static const std::string& text(const TextSuggestion& suggestion) {
    return suggestion.text;
  }

  static bool Read(SuggestionCandidateDataView input, TextSuggestion* output);
};

template <>
struct StructTraits<chromeos::ime::mojom::CompletionCandidateDataView,
                    chromeos::ime::TextCompletionCandidate> {
  using CompletionCandidateDataView =
      ::chromeos::ime::mojom::CompletionCandidateDataView;
  using TextCompletionCandidate = ::chromeos::ime::TextCompletionCandidate;

  static const std::string& text(const TextCompletionCandidate& candidate) {
    return candidate.text;
  }

  static const float& normalized_score(
      const TextCompletionCandidate& candidate) {
    return candidate.score;
  }

  static bool Read(CompletionCandidateDataView input,
                   TextCompletionCandidate* output);
};

}  // namespace mojo

#endif  // CHROMEOS_SERVICES_IME_PUBLIC_MOJOM_MOJOM_TRAITS_H_
