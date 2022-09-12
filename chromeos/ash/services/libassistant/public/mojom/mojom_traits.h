// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_PUBLIC_MOJOM_MOJOM_TRAITS_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_PUBLIC_MOJOM_MOJOM_TRAITS_H_

#include <cstdint>
#include <vector>

#include "base/containers/span.h"
#include "base/time/time.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_enums.h"
#include "chromeos/ash/services/libassistant/public/cpp/android_app_info.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_feedback.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_interaction_metadata.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_notification.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_suggestion.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_timer.h"
#include "chromeos/ash/services/libassistant/public/mojom/android_app_info.mojom-shared.h"
#include "chromeos/ash/services/libassistant/public/mojom/conversation_controller.mojom-shared.h"
#include "chromeos/ash/services/libassistant/public/mojom/conversation_observer.mojom-shared.h"
#include "chromeos/ash/services/libassistant/public/mojom/timer_controller.mojom-shared.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace mojo {

template <>
struct StructTraits<chromeos::libassistant::mojom::AndroidAppInfoDataView,
                    chromeos::assistant::AndroidAppInfo> {
  using AndroidAppInfo = chromeos::assistant::AndroidAppInfo;

  static const std::string& package_name(const AndroidAppInfo& input);
  static int64_t version(const AndroidAppInfo& input);
  static const std::string& localized_app_name(const AndroidAppInfo& input);
  static const std::string& intent(const AndroidAppInfo& input);
  static chromeos::assistant::AppStatus status(const AndroidAppInfo& input);
  static const std::string& action(const AndroidAppInfo& input);

  static bool Read(chromeos::libassistant::mojom::AndroidAppInfoDataView data,
                   AndroidAppInfo* output);
};

template <>
struct EnumTraits<chromeos::libassistant::mojom::AndroidAppStatus,
                  chromeos::assistant::AppStatus> {
  using AppStatus = ::chromeos::assistant::AppStatus;
  using AndroidAppStatus = ::chromeos::libassistant::mojom::AndroidAppStatus;

  static AndroidAppStatus ToMojom(AppStatus input);
  static bool FromMojom(AndroidAppStatus input, AppStatus* output);
};

template <>
struct StructTraits<
    chromeos::libassistant::mojom::AssistantNotificationDataView,
    chromeos::assistant::AssistantNotification> {
  using AssistantNotification = chromeos::assistant::AssistantNotification;

  static const std::string& title(const AssistantNotification& input);
  static const std::string& message(const AssistantNotification& input);
  static const GURL& action_url(const AssistantNotification& input);
  static const std::string& client_id(const AssistantNotification& input);
  static const std::string& server_id(const AssistantNotification& input);
  static const std::string& consistency_token(
      const AssistantNotification& input);
  static const std::string& opaque_token(const AssistantNotification& input);
  static const std::string& grouping_key(const AssistantNotification& input);
  static const std::string& obfuscated_gaia_id(
      const AssistantNotification& input);
  static const absl::optional<base::Time>& expiry_time(
      const AssistantNotification& input);
  static const std::vector<chromeos::assistant::AssistantNotificationButton>&
  buttons(const AssistantNotification& input);
  static bool from_server(const AssistantNotification& input);

  static bool Read(
      chromeos::libassistant::mojom::AssistantNotificationDataView data,
      AssistantNotification* output);
};

template <>
struct StructTraits<
    chromeos::libassistant::mojom::AssistantNotificationButtonDataView,
    chromeos::assistant::AssistantNotificationButton> {
  using AssistantNotificationButton =
      chromeos::assistant::AssistantNotificationButton;

  static const std::string& label(const AssistantNotificationButton& input);
  static const GURL& action_url(const AssistantNotificationButton& input);
  static bool remove_notification_on_click(
      const AssistantNotificationButton& input);

  static bool Read(
      chromeos::libassistant::mojom::AssistantNotificationButtonDataView data,
      AssistantNotificationButton* output);
};

template <>
struct StructTraits<chromeos::libassistant::mojom::AssistantFeedbackDataView,
                    chromeos::assistant::AssistantFeedback> {
  using AssistantFeedback = chromeos::assistant::AssistantFeedback;

  static const std::string& description(const AssistantFeedback& input);
  static bool assistant_debug_info_allowed(const AssistantFeedback& input);
  static base::span<const uint8_t> screenshot_png(
      const AssistantFeedback& input);

  static bool Read(
      chromeos::libassistant::mojom::AssistantFeedbackDataView data,
      AssistantFeedback* output);
};

template <>
struct StructTraits<
    chromeos::libassistant::mojom::AssistantInteractionMetadataDataView,
    chromeos::assistant::AssistantInteractionMetadata> {
  using NativeType = chromeos::assistant::AssistantInteractionMetadata;
  using MojomType =
      chromeos::libassistant::mojom::AssistantInteractionMetadataDataView;

  static chromeos::assistant::AssistantInteractionType type(
      const NativeType& input);
  static chromeos::assistant::AssistantQuerySource source(
      const NativeType& input);
  static const std::string& query(const NativeType& input);

  static bool Read(MojomType data, NativeType* output);
};

template <>
struct EnumTraits<chromeos::libassistant::mojom::AssistantInteractionResolution,
                  chromeos::assistant::AssistantInteractionResolution> {
  using NativeType = chromeos::assistant::AssistantInteractionResolution;
  using MojomType =
      chromeos::libassistant::mojom::AssistantInteractionResolution;

  static MojomType ToMojom(NativeType input);
  static bool FromMojom(MojomType input, NativeType* output);
};

template <>
struct EnumTraits<chromeos::libassistant::mojom::AssistantInteractionType,
                  chromeos::assistant::AssistantInteractionType> {
  using NativeType = chromeos::assistant::AssistantInteractionType;
  using MojomType = chromeos::libassistant::mojom::AssistantInteractionType;

  static MojomType ToMojom(NativeType input);
  static bool FromMojom(MojomType input, NativeType* output);
};

template <>
struct EnumTraits<chromeos::libassistant::mojom::AssistantQuerySource,
                  chromeos::assistant::AssistantQuerySource> {
  using NativeType = chromeos::assistant::AssistantQuerySource;
  using MojomType = chromeos::libassistant::mojom::AssistantQuerySource;

  static MojomType ToMojom(NativeType input);
  static bool FromMojom(MojomType input, NativeType* output);
};

template <>
struct StructTraits<chromeos::libassistant::mojom::AssistantSuggestionDataView,
                    chromeos::assistant::AssistantSuggestion> {
  using AssistantSuggestion = chromeos::assistant::AssistantSuggestion;

  static const base::UnguessableToken& id(const AssistantSuggestion& input);
  static chromeos::assistant::AssistantSuggestionType type(
      const AssistantSuggestion& input);
  static const std::string& text(const AssistantSuggestion& input);
  static const GURL& icon_url(const AssistantSuggestion& input);
  static const GURL& action_url(const AssistantSuggestion& input);

  static bool Read(
      chromeos::libassistant::mojom::AssistantSuggestionDataView data,
      AssistantSuggestion* output);
};

template <>
struct EnumTraits<chromeos::libassistant::mojom::AssistantSuggestionType,
                  chromeos::assistant::AssistantSuggestionType> {
  using AssistantSuggestionType = chromeos::assistant::AssistantSuggestionType;
  using MojoSuggestionType =
      chromeos::libassistant::mojom::AssistantSuggestionType;

  static MojoSuggestionType ToMojom(AssistantSuggestionType input);
  static bool FromMojom(MojoSuggestionType input,
                        AssistantSuggestionType* output);
};

template <>
struct StructTraits<chromeos::libassistant::mojom::AssistantTimerDataView,
                    chromeos::assistant::AssistantTimer> {
  using AssistantTimer = chromeos::assistant::AssistantTimer;

  static const std::string& id(const AssistantTimer& input);
  static const std::string& label(const AssistantTimer& input);
  static const base::Time& fire_time(const AssistantTimer& input);
  static const base::TimeDelta& original_duration(const AssistantTimer& input);
  static const base::TimeDelta& remaining_time(const AssistantTimer& input);
  static chromeos::assistant::AssistantTimerState state(
      const AssistantTimer& input);

  static bool Read(chromeos::libassistant::mojom::AssistantTimerDataView data,
                   AssistantTimer* output);
};

template <>
struct EnumTraits<chromeos::libassistant::mojom::AssistantTimerState,
                  chromeos::assistant::AssistantTimerState> {
  using AssistantTimerState = ::chromeos::assistant::AssistantTimerState;
  using MojomAssistantTimerState =
      ::chromeos::libassistant::mojom::AssistantTimerState;

  static MojomAssistantTimerState ToMojom(AssistantTimerState input);
  static bool FromMojom(MojomAssistantTimerState input,
                        AssistantTimerState* output);
};

}  // namespace mojo

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_PUBLIC_MOJOM_MOJOM_TRAITS_H_
