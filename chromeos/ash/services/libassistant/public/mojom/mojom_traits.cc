// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/public/mojom/mojom_traits.h"

#include "base/notreached.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
#include "url/gurl.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

using AssistantResolution = ::ash::assistant::AssistantInteractionResolution;
using MojoResolution =
    ::ash::libassistant::mojom::AssistantInteractionResolution;
using MojomInteractionType =
    ::ash::libassistant::mojom::AssistantInteractionType;
using MojomQuerySource = ::ash::libassistant::mojom::AssistantQuerySource;
using MojoSuggestionType = ::ash::libassistant::mojom::AssistantSuggestionType;
using ::ash::assistant::AndroidAppInfo;
using ::ash::assistant::AppStatus;
using ::ash::assistant::AssistantFeedback;
using ::ash::assistant::AssistantInteractionMetadata;
using ::ash::assistant::AssistantInteractionType;
using ::ash::assistant::AssistantNotification;
using ::ash::assistant::AssistantNotificationButton;
using ::ash::assistant::AssistantQuerySource;
using ::ash::assistant::AssistantSuggestion;
using ::ash::assistant::AssistantSuggestionType;
using ::ash::assistant::AssistantTimer;
using ::ash::assistant::AssistantTimerState;
using ::ash::libassistant::mojom::AndroidAppInfoDataView;
using ::ash::libassistant::mojom::AndroidAppStatus;
using ::ash::libassistant::mojom::AssistantFeedbackDataView;
using ::ash::libassistant::mojom::AssistantInteractionMetadataDataView;
using ::ash::libassistant::mojom::AssistantNotificationButtonDataView;
using ::ash::libassistant::mojom::AssistantNotificationDataView;
using ::ash::libassistant::mojom::AssistantSuggestionDataView;
using ::ash::libassistant::mojom::AssistantTimerDataView;
using MojomAssistantTimerState =
    ::ash::libassistant::mojom::AssistantTimerState;

////////////////////////////////////////////////////////////////////////////////
// AndroidAppStatus
////////////////////////////////////////////////////////////////////////////////

AndroidAppStatus EnumTraits<AndroidAppStatus, AppStatus>::ToMojom(
    AppStatus input) {
  switch (input) {
    case AppStatus::kUnknown:
      return AndroidAppStatus::kUnknown;
    case AppStatus::kAvailable:
      return AndroidAppStatus::kAvailable;
    case AppStatus::kUnavailable:
      return AndroidAppStatus::kUnavailable;
    case AppStatus::kVersionMismatch:
      return AndroidAppStatus::kVersionMismatch;
    case AppStatus::kDisabled:
      return AndroidAppStatus::kDisabled;
  }
}

bool EnumTraits<AndroidAppStatus, AppStatus>::FromMojom(AndroidAppStatus input,
                                                        AppStatus* output) {
  switch (input) {
    case AndroidAppStatus::kUnknown:
      *output = AppStatus::kUnknown;
      break;
    case AndroidAppStatus::kAvailable:
      *output = AppStatus::kAvailable;
      break;
    case AndroidAppStatus::kUnavailable:
      *output = AppStatus::kUnavailable;
      break;
    case AndroidAppStatus::kVersionMismatch:
      *output = AppStatus::kVersionMismatch;
      break;
    case AndroidAppStatus::kDisabled:
      *output = AppStatus::kDisabled;
      break;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// AndroidAppInfo
////////////////////////////////////////////////////////////////////////////////

const std::string&
StructTraits<AndroidAppInfoDataView, AndroidAppInfo>::package_name(
    const AndroidAppInfo& input) {
  return input.package_name;
}

int64_t StructTraits<AndroidAppInfoDataView, AndroidAppInfo>::version(
    const AndroidAppInfo& input) {
  return input.version;
}

const std::string&
StructTraits<AndroidAppInfoDataView, AndroidAppInfo>::localized_app_name(
    const AndroidAppInfo& input) {
  return input.localized_app_name;
}

const std::string& StructTraits<AndroidAppInfoDataView, AndroidAppInfo>::intent(
    const AndroidAppInfo& input) {
  return input.intent;
}

AppStatus StructTraits<AndroidAppInfoDataView, AndroidAppInfo>::status(
    const AndroidAppInfo& input) {
  return input.status;
}

const std::string& StructTraits<AndroidAppInfoDataView, AndroidAppInfo>::action(
    const AndroidAppInfo& input) {
  return input.action;
}

bool StructTraits<AndroidAppInfoDataView, AndroidAppInfo>::Read(
    AndroidAppInfoDataView data,
    AndroidAppInfo* output) {
  if (!data.ReadPackageName(&output->package_name))
    return false;
  output->version = data.version();
  if (!data.ReadLocalizedAppName(&output->localized_app_name))
    return false;
  if (!data.ReadIntent(&output->intent))
    return false;
  if (!data.ReadStatus(&output->status))
    return false;
  if (!data.ReadAction(&output->action))
    return false;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// AssistantNotification
////////////////////////////////////////////////////////////////////////////////

const std::string&
StructTraits<AssistantNotificationDataView, AssistantNotification>::title(
    const AssistantNotification& input) {
  return input.title;
}

const std::string&
StructTraits<AssistantNotificationDataView, AssistantNotification>::message(
    const AssistantNotification& input) {
  return input.message;
}

const GURL&
StructTraits<AssistantNotificationDataView, AssistantNotification>::action_url(
    const AssistantNotification& input) {
  return input.action_url;
}

const std::string&
StructTraits<AssistantNotificationDataView, AssistantNotification>::client_id(
    const AssistantNotification& input) {
  return input.client_id;
}

const std::string&
StructTraits<AssistantNotificationDataView, AssistantNotification>::server_id(
    const AssistantNotification& input) {
  return input.server_id;
}

const std::string&
StructTraits<AssistantNotificationDataView, AssistantNotification>::
    consistency_token(const AssistantNotification& input) {
  return input.consistency_token;
}

const std::string& StructTraits<
    AssistantNotificationDataView,
    AssistantNotification>::opaque_token(const AssistantNotification& input) {
  return input.opaque_token;
}

const std::string& StructTraits<
    AssistantNotificationDataView,
    AssistantNotification>::grouping_key(const AssistantNotification& input) {
  return input.grouping_key;
}

const std::string&
StructTraits<AssistantNotificationDataView, AssistantNotification>::
    obfuscated_gaia_id(const AssistantNotification& input) {
  return input.obfuscated_gaia_id;
}

const std::optional<base::Time>&
StructTraits<AssistantNotificationDataView, AssistantNotification>::expiry_time(
    const AssistantNotification& input) {
  return input.expiry_time;
}

const std::vector<AssistantNotificationButton>&
StructTraits<AssistantNotificationDataView, AssistantNotification>::buttons(
    const AssistantNotification& input) {
  return input.buttons;
}

bool StructTraits<AssistantNotificationDataView, AssistantNotification>::
    from_server(const AssistantNotification& input) {
  return input.from_server;
}

bool StructTraits<AssistantNotificationDataView, AssistantNotification>::Read(
    AssistantNotificationDataView data,
    AssistantNotification* output) {
  if (!data.ReadTitle(&output->title))
    return false;
  if (!data.ReadMessage(&output->message))
    return false;
  if (!data.ReadActionUrl(&output->action_url))
    return false;
  if (!data.ReadClientId(&output->client_id))
    return false;
  if (!data.ReadServerId(&output->server_id))
    return false;
  if (!data.ReadConsistencyToken(&output->consistency_token))
    return false;
  if (!data.ReadOpaqueToken(&output->opaque_token))
    return false;
  if (!data.ReadGroupingKey(&output->grouping_key))
    return false;
  if (!data.ReadObfuscatedGaiaId(&output->obfuscated_gaia_id))
    return false;
  if (!data.ReadExpiryTime(&output->expiry_time))
    return false;
  if (!data.ReadButtons(&output->buttons))
    return false;
  output->from_server = data.from_server();
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// AssistantNotificationButton
////////////////////////////////////////////////////////////////////////////////

const std::string&
StructTraits<AssistantNotificationButtonDataView, AssistantNotificationButton>::
    label(const AssistantNotificationButton& input) {
  return input.label;
}

const GURL&
StructTraits<AssistantNotificationButtonDataView, AssistantNotificationButton>::
    action_url(const AssistantNotificationButton& input) {
  return input.action_url;
}

bool StructTraits<AssistantNotificationButtonDataView,
                  AssistantNotificationButton>::
    remove_notification_on_click(const AssistantNotificationButton& input) {
  return input.remove_notification_on_click;
}

bool StructTraits<
    AssistantNotificationButtonDataView,
    AssistantNotificationButton>::Read(AssistantNotificationButtonDataView data,
                                       AssistantNotificationButton* output) {
  if (!data.ReadLabel(&output->label))
    return false;
  if (!data.ReadActionUrl(&output->action_url))
    return false;
  output->remove_notification_on_click = data.remove_notification_on_click();

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// AssistantFeedback
////////////////////////////////////////////////////////////////////////////////

const std::string&
StructTraits<AssistantFeedbackDataView, AssistantFeedback>::description(
    const AssistantFeedback& input) {
  return input.description;
}

bool StructTraits<AssistantFeedbackDataView, AssistantFeedback>::
    assistant_debug_info_allowed(const AssistantFeedback& input) {
  return input.assistant_debug_info_allowed;
}

base::span<const uint8_t>
StructTraits<AssistantFeedbackDataView, AssistantFeedback>::screenshot_png(
    const AssistantFeedback& input) {
  return input.screenshot_png;
}

bool StructTraits<AssistantFeedbackDataView, AssistantFeedback>::Read(
    AssistantFeedbackDataView data,
    AssistantFeedback* output) {
  if (!data.ReadDescription(&output->description))
    return false;
  output->assistant_debug_info_allowed = data.assistant_debug_info_allowed();
  if (!data.ReadScreenshotPng(&output->screenshot_png))
    return false;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// AssistantInteractionResolution
////////////////////////////////////////////////////////////////////////////////

MojoResolution EnumTraits<MojoResolution, AssistantResolution>::ToMojom(
    AssistantResolution input) {
  switch (input) {
    case AssistantResolution::kNormal:
      return MojoResolution::kNormal;
    case AssistantResolution::kInterruption:
      return MojoResolution::kInterruption;
    case AssistantResolution::kError:
      return MojoResolution::kError;
    case AssistantResolution::kMicTimeout:
      return MojoResolution::kMicTimeout;
    case AssistantResolution::kMultiDeviceHotwordLoss:
      return MojoResolution::kMultiDeviceHotwordLoss;
  }
  NOTREACHED_IN_MIGRATION();
  return MojoResolution::kNormal;
}

bool EnumTraits<MojoResolution, AssistantResolution>::FromMojom(
    MojoResolution input,
    AssistantResolution* output) {
  switch (input) {
    case MojoResolution::kNormal:
      *output = AssistantResolution::kNormal;
      return true;
    case MojoResolution::kInterruption:
      *output = AssistantResolution::kInterruption;
      return true;
    case MojoResolution::kError:
      *output = AssistantResolution::kError;
      return true;
    case MojoResolution::kMicTimeout:
      *output = AssistantResolution::kMicTimeout;
      return true;
    case MojoResolution::kMultiDeviceHotwordLoss:
      *output = AssistantResolution::kMultiDeviceHotwordLoss;
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// AssistantInteractionMetadata
////////////////////////////////////////////////////////////////////////////////

AssistantInteractionType
StructTraits<AssistantInteractionMetadataDataView,
             AssistantInteractionMetadata>::type(const NativeType& input) {
  return input.type;
}

AssistantQuerySource
StructTraits<AssistantInteractionMetadataDataView,
             AssistantInteractionMetadata>::source(const NativeType& input) {
  return input.source;
}

const std::string&
StructTraits<AssistantInteractionMetadataDataView,
             AssistantInteractionMetadata>::query(const NativeType& input) {
  return input.query;
}

bool StructTraits<AssistantInteractionMetadataDataView,
                  AssistantInteractionMetadata>::Read(MojomType data,
                                                      NativeType* output) {
  if (!data.ReadType(&output->type))
    return false;
  if (!data.ReadSource(&output->source))
    return false;
  if (!data.ReadQuery(&output->query))
    return false;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// AssistantInteractionType
////////////////////////////////////////////////////////////////////////////////

MojomInteractionType
EnumTraits<MojomInteractionType, AssistantInteractionType>::ToMojom(
    NativeType input) {
  switch (input) {
    case NativeType::kText:
      return MojomInteractionType::kText;
    case NativeType::kVoice:
      return MojomInteractionType::kVoice;
  }
}

bool EnumTraits<MojomInteractionType, AssistantInteractionType>::FromMojom(
    MojomType input,
    NativeType* output) {
  switch (input) {
    case MojomType::kText:
      *output = NativeType::kText;
      return true;
    case MojomType::kVoice:
      *output = NativeType::kVoice;
      return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// AssistantQuerySource
////////////////////////////////////////////////////////////////////////////////

MojomQuerySource EnumTraits<MojomQuerySource, AssistantQuerySource>::ToMojom(
    NativeType input) {
  switch (input) {
    case NativeType::kUnspecified:
      return MojomType::kUnspecified;
    case NativeType::kDeepLink:
      return MojomType::kDeepLink;
    case NativeType::kDialogPlateTextField:
      return MojomType::kDialogPlateTextField;
    case NativeType::kStylus:
      return MojomType::kStylus;
    case NativeType::kSuggestionChip:
      return MojomType::kSuggestionChip;
    case NativeType::kVoiceInput:
      return MojomType::kVoiceInput;
    case NativeType::kLibAssistantInitiated:
      return MojomType::kLibAssistantInitiated;
    case NativeType::kConversationStarter:
      return MojomType::kConversationStarter;
    case NativeType::kWhatsOnMyScreen:
      return MojomType::kWhatsOnMyScreen;
    case NativeType::kQuickAnswers:
      return MojomType::kQuickAnswers;
    case NativeType::kLauncherChip:
      return MojomType::kLauncherChip;
    case NativeType::kBetterOnboarding:
      return MojomType::kBetterOnboarding;
  }
}

bool EnumTraits<MojomQuerySource, AssistantQuerySource>::FromMojom(
    MojomType input,
    NativeType* output) {
  switch (input) {
    case MojomType::kUnspecified:
      *output = NativeType::kUnspecified;
      return true;
    case MojomType::kDeepLink:
      *output = NativeType::kDeepLink;
      return true;
    case MojomType::kDialogPlateTextField:
      *output = NativeType::kDialogPlateTextField;
      return true;
    case MojomType::kStylus:
      *output = NativeType::kStylus;
      return true;
    case MojomType::kSuggestionChip:
      *output = NativeType::kSuggestionChip;
      return true;
    case MojomType::kVoiceInput:
      *output = NativeType::kVoiceInput;
      return true;
    case MojomType::kLibAssistantInitiated:
      *output = NativeType::kLibAssistantInitiated;
      return true;
    case MojomType::kConversationStarter:
      *output = NativeType::kConversationStarter;
      return true;
    case MojomType::kWhatsOnMyScreen:
      *output = NativeType::kWhatsOnMyScreen;
      return true;
    case MojomType::kQuickAnswers:
      *output = NativeType::kQuickAnswers;
      return true;
    case MojomType::kLauncherChip:
      *output = NativeType::kLauncherChip;
      return true;
    case MojomType::kBetterOnboarding:
      *output = NativeType::kBetterOnboarding;
      return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// AssistantSuggestion
////////////////////////////////////////////////////////////////////////////////

const base::UnguessableToken&
StructTraits<AssistantSuggestionDataView, AssistantSuggestion>::id(
    const AssistantSuggestion& input) {
  return input.id;
}

AssistantSuggestionType
StructTraits<AssistantSuggestionDataView, AssistantSuggestion>::type(
    const AssistantSuggestion& input) {
  return input.type;
}

const std::string&
StructTraits<AssistantSuggestionDataView, AssistantSuggestion>::text(
    const AssistantSuggestion& input) {
  return input.text;
}

const GURL&
StructTraits<AssistantSuggestionDataView, AssistantSuggestion>::icon_url(
    const AssistantSuggestion& input) {
  return input.icon_url;
}

const GURL&
StructTraits<AssistantSuggestionDataView, AssistantSuggestion>::action_url(
    const AssistantSuggestion& input) {
  return input.action_url;
}

bool StructTraits<AssistantSuggestionDataView, AssistantSuggestion>::Read(
    AssistantSuggestionDataView data,
    AssistantSuggestion* output) {
  if (!data.ReadId(&output->id))
    return false;
  if (!data.ReadType(&output->type))
    return false;
  if (!data.ReadText(&output->text))
    return false;
  if (!data.ReadIconUrl(&output->icon_url))
    return false;
  if (!data.ReadActionUrl(&output->action_url))
    return false;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// AssistantSuggestionType
////////////////////////////////////////////////////////////////////////////////

MojoSuggestionType
EnumTraits<MojoSuggestionType, AssistantSuggestionType>::ToMojom(
    AssistantSuggestionType input) {
  switch (input) {
    case AssistantSuggestionType::kUnspecified:
      return MojoSuggestionType::kUnspecified;
    case AssistantSuggestionType::kConversationStarter:
      return MojoSuggestionType::kConversationStarter;
    case AssistantSuggestionType::kBetterOnboarding:
      return MojoSuggestionType::kBetterOnboarding;
  }
  NOTREACHED_IN_MIGRATION();
  return MojoSuggestionType::kUnspecified;
}

bool EnumTraits<MojoSuggestionType, AssistantSuggestionType>::FromMojom(
    MojoSuggestionType input,
    AssistantSuggestionType* output) {
  switch (input) {
    case MojoSuggestionType::kUnspecified:
      *output = AssistantSuggestionType::kUnspecified;
      return true;
    case MojoSuggestionType::kConversationStarter:
      *output = AssistantSuggestionType::kConversationStarter;
      return true;
    case MojoSuggestionType::kBetterOnboarding:
      *output = AssistantSuggestionType::kBetterOnboarding;
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// AssistantTimer
////////////////////////////////////////////////////////////////////////////////

const std::string& StructTraits<AssistantTimerDataView, AssistantTimer>::id(
    const AssistantTimer& input) {
  return input.id;
}

const std::string& StructTraits<AssistantTimerDataView, AssistantTimer>::label(
    const AssistantTimer& input) {
  return input.label;
}

const base::Time&
StructTraits<AssistantTimerDataView, AssistantTimer>::fire_time(
    const AssistantTimer& input) {
  return input.fire_time;
}

const base::TimeDelta&
StructTraits<AssistantTimerDataView, AssistantTimer>::original_duration(
    const AssistantTimer& input) {
  return input.original_duration;
}

const base::TimeDelta&
StructTraits<AssistantTimerDataView, AssistantTimer>::remaining_time(
    const AssistantTimer& input) {
  return input.remaining_time;
}

AssistantTimerState StructTraits<AssistantTimerDataView, AssistantTimer>::state(
    const AssistantTimer& input) {
  return input.state;
}

bool StructTraits<AssistantTimerDataView, AssistantTimer>::Read(
    AssistantTimerDataView data,
    AssistantTimer* output) {
  if (!data.ReadId(&output->id))
    return false;
  if (!data.ReadLabel(&output->label))
    return false;
  if (!data.ReadFireTime(&output->fire_time))
    return false;
  if (!data.ReadOriginalDuration(&output->original_duration))
    return false;
  if (!data.ReadRemainingTime(&output->remaining_time))
    return false;
  if (!data.ReadState(&output->state))
    return false;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// AssistantTimerState
////////////////////////////////////////////////////////////////////////////////

MojomAssistantTimerState
EnumTraits<MojomAssistantTimerState, AssistantTimerState>::ToMojom(
    AssistantTimerState input) {
  switch (input) {
    case AssistantTimerState::kUnknown:
      return MojomAssistantTimerState::kUnknown;
    case AssistantTimerState::kScheduled:
      return MojomAssistantTimerState::kScheduled;
    case AssistantTimerState::kPaused:
      return MojomAssistantTimerState::kPaused;
    case AssistantTimerState::kFired:
      return MojomAssistantTimerState::kFired;
  }
}

bool EnumTraits<MojomAssistantTimerState, AssistantTimerState>::FromMojom(
    MojomAssistantTimerState input,
    AssistantTimerState* output) {
  switch (input) {
    case MojomAssistantTimerState::kUnknown:
      *output = AssistantTimerState::kUnknown;
      break;
    case MojomAssistantTimerState::kScheduled:
      *output = AssistantTimerState::kScheduled;
      break;
    case MojomAssistantTimerState::kPaused:
      *output = AssistantTimerState::kPaused;
      break;
    case MojomAssistantTimerState::kFired:
      *output = AssistantTimerState::kFired;
      break;
  }
  return true;
}

}  // namespace mojo
