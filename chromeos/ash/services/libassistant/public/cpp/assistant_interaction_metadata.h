// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_PUBLIC_CPP_ASSISTANT_INTERACTION_METADATA_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_PUBLIC_CPP_ASSISTANT_INTERACTION_METADATA_H_

#include <string>

#include "base/component_export.h"

namespace ash::assistant {

// Enumeration of possible Assistant query sources. These values are persisted
// to logs. Entries should not be renumbered and numeric values should never
// be reused. Append new values to the end.
enum class AssistantQuerySource {
  kUnspecified = 0,
  kMinValue = kUnspecified,
  kDeepLink = 1,
  kDialogPlateTextField = 2,
  kStylus = 3,
  kSuggestionChip = 4,
  kVoiceInput = 5,
  // kProactiveSuggestionsDeprecated = 6,
  kLibAssistantInitiated = 7,
  // kWarmerWelcomeDeprecated = 8,
  kConversationStarter = 9,
  kWhatsOnMyScreen = 10,
  kQuickAnswers = 11,
  kLauncherChip = 12,
  // `kBetterOnboarding` is deprecated: https://crrev.com/c/5536248.
  kBetterOnboarding = 13,
  // kBloomDeprecated = 14,
  kMaxValue = kBetterOnboarding,
};

// Enumeration of possible Assistant interaction types.
enum class AssistantInteractionType {
  kText,
  kVoice,
};

// Describes an Assistant interaction.
struct COMPONENT_EXPORT(LIBASSISTANT_PUBLIC_STRUCTS)
    AssistantInteractionMetadata {
  AssistantInteractionMetadata();
  AssistantInteractionMetadata(AssistantInteractionType type,
                               AssistantQuerySource source,
                               const std::string& query);
  AssistantInteractionMetadata(const AssistantInteractionMetadata& suggestion);
  AssistantInteractionMetadata& operator=(const AssistantInteractionMetadata&);
  AssistantInteractionMetadata(AssistantInteractionMetadata&& suggestion);
  AssistantInteractionMetadata& operator=(AssistantInteractionMetadata&&);
  ~AssistantInteractionMetadata();

  AssistantInteractionType type{AssistantInteractionType::kText};
  AssistantQuerySource source{AssistantQuerySource::kUnspecified};
  std::string query;
};

}  // namespace ash::assistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_PUBLIC_CPP_ASSISTANT_INTERACTION_METADATA_H_
