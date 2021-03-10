// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/conversation_controller.h"

#include <memory>

#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/thread_annotations.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromeos/assistant/internal/action/cros_action_module.h"
#include "chromeos/assistant/internal/internal_util.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "chromeos/services/assistant/public/cpp/migration/libassistant_v1_api.h"
#include "chromeos/services/libassistant/public/mojom/conversation_controller.mojom.h"
#include "chromeos/services/libassistant/service_controller.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "libassistant/shared/internal_api/assistant_manager_delegate.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace libassistant {

using assistant::AssistantInteractionMetadata;
using assistant::AssistantInteractionType;
using assistant::AssistantQuerySource;

namespace {
// A macro which ensures we are running on the main thread.
#define ENSURE_MOJOM_THREAD(method, ...)                                    \
  if (!mojom_task_runner_->RunsTasksInCurrentSequence()) {                  \
    mojom_task_runner_->PostTask(                                           \
        FROM_HERE,                                                          \
        base::BindOnce(method, weak_factory_.GetWeakPtr(), ##__VA_ARGS__)); \
    return;                                                                 \
  }

// Helper function to convert |action::Suggestion| to |AssistantSuggestion|.
std::vector<assistant::AssistantSuggestion> ToAssistantSuggestion(
    const std::vector<assistant::action::Suggestion>& suggestions) {
  std::vector<assistant::AssistantSuggestion> result;
  for (const auto& suggestion : suggestions) {
    assistant::AssistantSuggestion assistant_suggestion;
    assistant_suggestion.id = base::UnguessableToken::Create();
    assistant_suggestion.text = suggestion.text;
    assistant_suggestion.icon_url = GURL(suggestion.icon_url);
    assistant_suggestion.action_url = GURL(suggestion.action_url);
    result.push_back(std::move(assistant_suggestion));
  }

  return result;
}
}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AssistantManagerDelegateImpl
////////////////////////////////////////////////////////////////////////////////

// Implementation of |AssistantManagerDelegate| that will forward all calls
// to the correct observers.
// It also keeps track of the last text query that was started, so we can
// pass its metadata to |OnConversationTurnStarted|.
class ConversationController::AssistantManagerDelegateImpl
    : public assistant_client::AssistantManagerDelegate {
 public:
  explicit AssistantManagerDelegateImpl(ConversationController* parent)
      : parent_(*parent),
        mojom_task_runner_(base::SequencedTaskRunnerHandle::Get()) {}
  AssistantManagerDelegateImpl(const AssistantManagerDelegateImpl&) = delete;
  AssistantManagerDelegateImpl& operator=(const AssistantManagerDelegateImpl&) =
      delete;
  ~AssistantManagerDelegateImpl() override = default;

  std::string AddPendingTextInteraction(const std::string& query,
                                        AssistantQuerySource source) {
    return NewPendingInteraction(AssistantInteractionType::kText, source,
                                 query);
  }

  std::string NewPendingInteraction(AssistantInteractionType interaction_type,
                                    AssistantQuerySource source,
                                    const std::string& query) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto id = base::NumberToString(next_interaction_id_++);
    pending_interactions_.emplace(
        id, AssistantInteractionMetadata(interaction_type, source, query));
    return id;
  }

  // assistant_client::AssistantManagerDelegate overrides:
  void OnConversationTurnStartedInternal(
      const assistant_client::ConversationTurnMetadata& metadata) override {
    ENSURE_MOJOM_THREAD(
        &AssistantManagerDelegateImpl::OnConversationTurnStartedInternal,
        metadata);
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Retrieve the cached interaction metadata associated with this
    // conversation turn or construct a new instance if there's no match in the
    // cache.
    AssistantInteractionMetadata interaction_metadata;
    auto it = pending_interactions_.find(metadata.id);
    if (it != pending_interactions_.end()) {
      interaction_metadata = it->second;
      pending_interactions_.erase(it);
    } else {
      interaction_metadata.type = metadata.is_mic_open
                                      ? AssistantInteractionType::kVoice
                                      : AssistantInteractionType::kText;
      interaction_metadata.source =
          AssistantQuerySource::kLibAssistantInitiated;
    }

    for (auto& observer : parent_.observers_)
      observer->OnInteractionStarted(interaction_metadata);
  }

  void OnNotificationRemoved(const std::string& grouping_key) override {
    ENSURE_MOJOM_THREAD(&AssistantManagerDelegateImpl::OnNotificationRemoved,
                        grouping_key);

    if (grouping_key.empty())
      RemoveAllNotifications();
    else
      RemoveNotification(grouping_key);
  }

  void OnCommunicationError(int error_code) override {
    ENSURE_MOJOM_THREAD(&AssistantManagerDelegateImpl::OnCommunicationError,
                        error_code);
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (assistant::IsAuthError(error_code)) {
      for (auto& observer : parent_.authentication_state_observers_)
        observer->OnAuthenticationError();
    }
  }

 private:
  void RemoveAllNotifications() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    for (auto& observer : parent_.observers_)
      observer->OnAllNotificationsRemoved();
  }

  void RemoveNotification(const std::string& id) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    for (auto& observer : parent_.observers_)
      observer->OnNotificationRemoved(id);
  }

  SEQUENCE_CHECKER(sequence_checker_);

  int next_interaction_id_ GUARDED_BY_CONTEXT(sequence_checker_) = 1;
  std::map<std::string, AssistantInteractionMetadata> pending_interactions_
      GUARDED_BY_CONTEXT(sequence_checker_);

  ConversationController& parent_ GUARDED_BY_CONTEXT(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> mojom_task_runner_;
  base::WeakPtrFactory<AssistantManagerDelegateImpl> weak_factory_{this};
};

////////////////////////////////////////////////////////////////////////////////
// ConversationController
////////////////////////////////////////////////////////////////////////////////

ConversationController::ConversationController(
    ServiceController* service_controller)
    : receiver_(this),
      service_controller_(service_controller),
      assistant_manager_delegate_(
          std::make_unique<AssistantManagerDelegateImpl>(this)),
      action_module_(std::make_unique<assistant::action::CrosActionModule>(
          assistant::features::IsAppSupportEnabled(),
          assistant::features::IsWaitSchedulingEnabled())),
      mojom_task_runner_(base::SequencedTaskRunnerHandle::Get()) {
  // TODO(jeroendh): We should not pass in the |ServiceController| into this
  // constructor. Instead, we should access the |AssistantManager| through
  // the methods offered by |AssistantManagerObserver|.
  DCHECK(service_controller_);
  action_module_->AddObserver(this);
}

ConversationController::~ConversationController() = default;

void ConversationController::Bind(
    mojo::PendingReceiver<mojom::ConversationController> receiver) {
  // Cannot bind the receiver twice.
  DCHECK(!receiver_.is_bound());
  receiver_.Bind(std::move(receiver));
}

void ConversationController::AddActionObserver(
    chromeos::assistant::action::AssistantActionObserver* observer) {
  action_module_->AddObserver(observer);
}

void ConversationController::AddAuthenticationStateObserver(
    mojo::PendingRemote<
        chromeos::libassistant::mojom::AuthenticationStateObserver> observer) {
  authentication_state_observers_.Add(std::move(observer));
}

void ConversationController::OnAssistantManagerCreated(
    assistant_client::AssistantManager* assistant_manager,
    assistant_client::AssistantManagerInternal* assistant_manager_internal) {
  // Registers ActionModule when AssistantManagerInternal has been created
  // but not yet started.
  assistant_manager_internal->RegisterActionModule(action_module_.get());

  assistant_manager_internal->SetAssistantManagerDelegate(
      assistant_manager_delegate_.get());

  auto* v1_api = assistant::LibassistantV1Api::Get();
  // LibassistantV1Api should be ready to use by this time.
  DCHECK(v1_api);

  v1_api->SetActionModule(action_module_.get());
}

void ConversationController::SendTextQuery(const std::string& query,
                                           AssistantQuerySource source,
                                           bool allow_tts) {
  // DCHECKs if this function gets invoked after the service has been fully
  // started.
  // TODO(meilinw): only check for the |ServiceState::kRunning| state instead
  // after it has been wired up.
  DCHECK(service_controller_->IsStarted())
      << "Libassistant service is not ready to handle queries.";
  DCHECK(assistant_manager_internal());

  // Configs |VoicelessOptions|.
  assistant_client::VoicelessOptions options;
  options.is_user_initiated = true;
  if (!allow_tts) {
    options.modality =
        assistant_client::VoicelessOptions::Modality::TYPING_MODALITY;
  }
  // Remember the interaction metadata, and pass the generated conversation id
  // to LibAssistant.
  options.conversation_turn_id =
      assistant_manager_delegate_->AddPendingTextInteraction(query, source);

  // Builds text interaction.
  std::string interaction = assistant::CreateTextQueryInteraction(query);

  assistant_manager_internal()->SendVoicelessInteraction(
      interaction, /*description=*/"text_query", options, [](auto) {});
}

void ConversationController::StartEditReminderInteraction(
    const std::string& client_id) {
  SendVoicelessInteraction(assistant::CreateEditReminderInteraction(client_id),
                           /*description=*/std::string(),
                           /*is_user_initiated=*/true);
}

void ConversationController::RetrieveNotification(
    const AssistantNotification& notification,
    int32_t action_index) {
  const std::string request_interaction =
      assistant::SerializeNotificationRequestInteraction(
          notification.server_id, notification.consistency_token,
          notification.opaque_token, action_index);

  SendVoicelessInteraction(request_interaction,
                           /*description=*/"RequestNotification",
                           /*is_user_initiated=*/true);
}

void ConversationController::DismissNotification(
    const AssistantNotification& notification) {
  // |assistant_manager_internal()| may not exist if we are dismissing
  // notifications as part of a shutdown sequence.
  if (!assistant_manager_internal())
    return;

  const std::string dismissed_interaction =
      assistant::SerializeNotificationDismissedInteraction(
          notification.server_id, notification.consistency_token,
          notification.opaque_token, {notification.grouping_key});

  assistant_client::VoicelessOptions options;
  options.obfuscated_gaia_id = notification.obfuscated_gaia_id;

  assistant_manager_internal()->SendVoicelessInteraction(
      dismissed_interaction, /*description=*/"DismissNotification", options,
      [](auto) {});
}

void ConversationController::SendAssistantFeedback(
    const AssistantFeedback& feedback) {
  std::string raw_image_data(feedback.screenshot_png.begin(),
                             feedback.screenshot_png.end());
  const std::string interaction = assistant::CreateSendFeedbackInteraction(
      feedback.assistant_debug_info_allowed, feedback.description,
      raw_image_data);

  SendVoicelessInteraction(interaction,
                           /*description=*/"send feedback with details",
                           /*is_user_initiated=*/false);
}

void ConversationController::AddRemoteObserver(
    mojo::PendingRemote<mojom::ConversationObserver> observer) {
  observers_.Add(std::move(observer));
}

// Called from Libassistant thread.
void ConversationController::OnShowHtml(const std::string& html_content,
                                        const std::string& fallback) {
  ENSURE_MOJOM_THREAD(&ConversationController::OnShowHtml, html_content,
                      fallback);

  for (auto& observer : observers_)
    observer->OnHtmlResponse(html_content, fallback);
}

// Called from Libassistant thread.
void ConversationController::OnShowText(const std::string& text) {
  ENSURE_MOJOM_THREAD(&ConversationController::OnShowText, text);

  for (auto& observer : observers_)
    observer->OnTextResponse(text);
}

// Called from Libassistant thread.
// Note that we should deprecate this API when the server provides a fallback.
void ConversationController::OnShowContextualQueryFallback() {
  // Show fallback message.
  OnShowText(l10n_util::GetStringUTF8(
      IDS_ASSISTANT_SCREEN_CONTEXT_QUERY_FALLBACK_TEXT));
}

void ConversationController::OnShowSuggestions(
    const std::vector<assistant::action::Suggestion>& suggestions) {
  ENSURE_MOJOM_THREAD(&ConversationController::OnShowSuggestions, suggestions);

  for (auto& observer : observers_)
    observer->OnSuggestionsResponse(ToAssistantSuggestion(suggestions));
}

// Called from Libassistant thread.
void ConversationController::OnOpenUrl(const std::string& url,
                                       bool in_background) {
  ENSURE_MOJOM_THREAD(&ConversationController::OnOpenUrl, url, in_background);

  for (auto& observer : observers_)
    observer->OnOpenUrlResponse(GURL(url), in_background);
}

void ConversationController::OnOpenAndroidApp(
    const chromeos::assistant::AndroidAppInfo& app_info,
    const chromeos::assistant::InteractionInfo& interaction) {
  ENSURE_MOJOM_THREAD(&ConversationController::OnOpenAndroidApp, app_info,
                      interaction);

  for (auto& observer : observers_)
    observer->OnOpenAppResponse(app_info);

  // Note that we will always set |provider_found| to true since the preceding
  // OnVerifyAndroidApp() should already confirm that the requested provider is
  // available on the device.
  std::string interaction_proto =
      assistant::CreateOpenProviderResponseInteraction(
          interaction.interaction_id, /*provider_found=*/true);
  assistant_client::VoicelessOptions options;
  options.obfuscated_gaia_id = interaction.user_id;

  assistant_manager_internal()->SendVoicelessInteraction(
      interaction_proto, /*description=*/"open_provider_response", options,
      [](auto) {});
}

void ConversationController::OnScheduleWait(int id, int time_ms) {
  ENSURE_MOJOM_THREAD(&ConversationController::OnScheduleWait, id, time_ms);

  DCHECK(assistant::features::IsWaitSchedulingEnabled());

  // Schedule a wait for |time_ms|, notifying the CrosActionModule when the wait
  // has finished so that it can inform LibAssistant to resume execution.
  mojom_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](const base::WeakPtr<ConversationController>& weak_ptr, int id) {
            if (weak_ptr) {
              weak_ptr->action_module_->OnScheduledWaitDone(
                  id, /*cancelled=*/false);
            }
          },
          weak_factory_.GetWeakPtr(), id),
      base::TimeDelta::FromMilliseconds(time_ms));

  // Notify subscribers that a wait has been started.
  for (auto& observer : observers_)
    observer->OnWaitStarted();
}

void ConversationController::SendVoicelessInteraction(
    const std::string& interaction,
    const std::string& description,
    bool is_user_initiated) {
  assistant_client::VoicelessOptions voiceless_options;
  voiceless_options.is_user_initiated = is_user_initiated;

  assistant_manager_internal()->SendVoicelessInteraction(
      interaction, description, voiceless_options, [](auto) {});
}

assistant_client::AssistantManagerInternal*
ConversationController::assistant_manager_internal() {
  return service_controller_->assistant_manager_internal();
}

}  // namespace libassistant
}  // namespace chromeos
