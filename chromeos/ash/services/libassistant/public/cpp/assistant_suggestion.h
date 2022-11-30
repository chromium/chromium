// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_PUBLIC_CPP_ASSISTANT_SUGGESTION_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_PUBLIC_CPP_ASSISTANT_SUGGESTION_H_

#include "base/component_export.h"
#include "base/unguessable_token.h"
#include "url/gurl.h"

namespace ash::assistant {

// Enumeration of possible Assistant suggestion types.
enum class AssistantSuggestionType {
  kUnspecified,
  kConversationStarter,
  kBetterOnboarding,
};

// Enumeration of better onboarding options. This is reported to histogram,
// please do not change the values.
enum class AssistantBetterOnboardingType {
  kUnspecified = 0,
  kMath = 1,
  kKnowledgeEdu = 2,
  kConversion = 3,
  kKnowledge = 4,
  kProductivity = 5,
  kPersonality = 6,
  kLanguage = 7,
  kTechnical = 8,
  kMaxValue = kTechnical,
};

// Models an Assistant suggestion.
struct COMPONENT_EXPORT(LIBASSISTANT_PUBLIC_STRUCTS) AssistantSuggestion {
  AssistantSuggestion();
  AssistantSuggestion(base::UnguessableToken id,
                      AssistantSuggestionType type,
                      const std::string& text);
  AssistantSuggestion(const AssistantSuggestion& suggestion);
  AssistantSuggestion& operator=(const AssistantSuggestion&);
  AssistantSuggestion(AssistantSuggestion&& suggestion);
  AssistantSuggestion& operator=(AssistantSuggestion&&);
  ~AssistantSuggestion();

  // Uniquely identifies a given suggestion.
  base::UnguessableToken id;

  // Allows us to differentiate between a typical Assistant suggestion and an
  // Assistant suggestion of another type (e.g. a conversation starter).
  AssistantSuggestionType type{AssistantSuggestionType::kUnspecified};

  AssistantBetterOnboardingType better_onboarding_type{
      AssistantBetterOnboardingType::kUnspecified};

  // Display text. e.g. "Cancel".
  std::string text;

  // Optional URL for icon. e.g. "https://www.gstatic.com/icon.png".
  // NOTE: This may be an icon resource link. e.g.
  // "googleassistant://resource?type=icon&name=assistant".
  GURL icon_url;

  // Optional URL for action. e.g.
  // "https://www.google.com/search?query=action".
  GURL action_url;
};

}  // namespace ash::assistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_PUBLIC_CPP_ASSISTANT_SUGGESTION_H_
