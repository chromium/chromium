// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_IME_PUBLIC_CPP_ASSISTIVE_SUGGESTIONS_H_
#define CHROMEOS_ASH_SERVICES_IME_PUBLIC_CPP_ASSISTIVE_SUGGESTIONS_H_

#include <string>
#include <vector>

namespace ash {
namespace ime {

// How should a consumer treat the given suggestion candidate; Should they
// complete the current unfinished word? Should they append the suggestion
// to the end of the user's text? Should they replace some text?
enum class AssistiveSuggestionMode {
  kPrediction = 0,
  kCompletion = 1,
};

// Specifies the types of assistive text suggestions the system can display to
// the user.
enum class AssistiveSuggestionType {
  kAssistivePersonalInfo = 0,
  kAssistiveEmoji = 1,
  kMultiWord = 2,
  kGrammar = 3,
  kLongpressDiacritic = 4,
};

// Encapsulates a single assistive suggestion that is displayed by the system
// to the user.
struct AssistiveSuggestion {
  AssistiveSuggestionMode mode;
  AssistiveSuggestionType type;
  std::string text;
  size_t confirmed_length;

  bool operator==(const AssistiveSuggestion& rhs) const {
    return (mode == rhs.mode && type == rhs.type && text == rhs.text &&
            confirmed_length == rhs.confirmed_length);
  }
};

// Holds the surrounding text context used to generate some suggestions.
struct SuggestionsTextContext {
  // The last N chars found in the surrounding text.
  std::string last_n_chars;
  // The full surrounding text length.
  size_t surrounding_text_length;
};

// Encapsulates a completion candidate emitted by a decoder.
struct DecoderCompletionCandidate {
  std::string text;
  float score;
};

// Specifies the current assistive window type being shown to the user.
enum class AssistiveWindowType {
  kNone,
  kUndoWindow,
  kEmojiSuggestion,
  kPersonalInfoSuggestion,
  kGrammarSuggestion,
  kMultiWordSuggestion,
  kLongpressDiacriticsSuggestion,
  kLearnMore,
};

// Represents the current state of suggestions in the assistive window.
struct AssistiveWindow {
  AssistiveWindow();
  ~AssistiveWindow();

  AssistiveWindowType type;
  // Holds one or more candidates shown in the assistive window. When a single
  // candidate is shown in the window this list will contain exactly one
  // item, when multiple candidates are shown in the window it will contain
  // multiple items.
  std::vector<AssistiveSuggestion> candidates;
};

}  // namespace ime
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_IME_PUBLIC_CPP_ASSISTIVE_SUGGESTIONS_H_
