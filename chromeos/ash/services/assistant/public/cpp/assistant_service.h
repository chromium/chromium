// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_SERVICE_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_SERVICE_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_enums.h"
#include "chromeos/ash/services/assistant/public/cpp/conversation_observer.h"
#include "chromeos/ash/services/libassistant/public/cpp/android_app_info.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_interaction_metadata.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_notification.h"
#include "chromeos/ash/services/libassistant/public/mojom/notification_delegate.mojom-forward.h"
#include "chromeos/ash/services/libassistant/public/mojom/notification_delegate.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/accessibility/mojom/ax_assistant_structure.mojom.h"

namespace ash::assistant {

struct AssistantFeedback;

// Subscribes to Assistant's interaction event. These events are server driven
// in response to the user's direct interaction with the assistant. Responses
// from the assistant may contain untrusted third-party content. Subscriber
// implementations must be sure to handle the response data appropriately.
class COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) AssistantInteractionSubscriber
    : public base::CheckedObserver,
      public ConversationObserver {
 public:
  AssistantInteractionSubscriber() = default;
  AssistantInteractionSubscriber(const AssistantInteractionSubscriber&) =
      delete;
  AssistantInteractionSubscriber& operator=(
      const AssistantInteractionSubscriber&) = delete;
  ~AssistantInteractionSubscriber() override = default;

  // Assistant speech recognition has started.
  virtual void OnSpeechRecognitionStarted() {}

  // Assistant speech recognition intermediate result is available.
  virtual void OnSpeechRecognitionIntermediateResult(
      const std::string& high_confidence_text,
      const std::string& low_confidence_text) {}

  // Assistant speech recognition detected end of utterance.
  virtual void OnSpeechRecognitionEndOfUtterance() {}

  // Assistant speech recognition final result is available.
  virtual void OnSpeechRecognitionFinalResult(const std::string& final_result) {
  }

  // Assistant got an instantaneous speech level update in dB.
  virtual void OnSpeechLevelUpdated(float speech_level) {}
};

class COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) Assistant {
 public:
  Assistant() = default;
  Assistant(const Assistant&) = delete;
  Assistant& operator=(const Assistant&) = delete;
  virtual ~Assistant() = default;

  // Starts an interaction to edit the reminder uniquely identified by
  // |client_id|. In response to the request, LibAssistant will initiate
  // a user facing interaction with the context pre-populated with details
  // to edit the specified reminder.
  virtual void StartEditReminderInteraction(const std::string& client_id) = 0;

  // Starts a new Assistant text interaction. If |allow_tts| is true, the
  // result will contain TTS. Otherwise TTS will not be present in the
  // generated server response. Results will be returned through registered
  // |AssistantInteractionSubscriber|.
  virtual void StartTextInteraction(const std::string& query,
                                    AssistantQuerySource source,
                                    bool allow_tts) = 0;

  // Starts a new Assistant voice interaction.
  virtual void StartVoiceInteraction() = 0;

  // Stops the active Assistant interaction and cancel the conversation if
  // |cancel_conversation|. If there is no active interaction, this method
  // is a no-op.
  virtual void StopActiveInteraction(bool cancel_conversation) = 0;

  // Registers assistant interaction event subscriber. Subscribers'
  // implementation is responsible for selecting events of interest.
  virtual void AddAssistantInteractionSubscriber(
      AssistantInteractionSubscriber* subscriber) = 0;
  virtual void RemoveAssistantInteractionSubscriber(
      AssistantInteractionSubscriber* subscriber) = 0;
  virtual void AddRemoteConversationObserver(
      ConversationObserver* observer) = 0;

  virtual mojo::PendingReceiver<libassistant::mojom::NotificationDelegate>
  GetPendingNotificationDelegate() = 0;

  // Retrieves a notification. A voiceless interaction will be sent to server to
  // retrieve the notification of |action_index|, which can trigger other
  // Assistant events such as OnTextResponse to show the result in the UI. The
  // retrieved notification will be removed from the UI.
  // |action_index| is the index of the tapped action. The main UI in the
  // notification contains the top level action, which index is 0. The buttons
  // have the additional actions, which are indexed starting from 1.
  virtual void RetrieveNotification(const AssistantNotification& notification,
                                    int action_index) = 0;

  // Dismisses a notification.
  virtual void DismissNotification(
      const AssistantNotification& notification) = 0;

  // Invoked when accessibility status is changed. Note that though
  // accessibility status has changed, |spoken_feedback_enabled| may not have.
  virtual void OnAccessibilityStatusChanged(bool spoken_feedback_enabled) = 0;

  // Invoked when color mode (dark/light mode) status changed.
  virtual void OnColorModeChanged(bool dark_mode_enabled) = 0;

  // Send Assistant feedback to Assistant server.
  virtual void SendAssistantFeedback(const AssistantFeedback& feedback) = 0;

  // Alarm/Timer methods -------------------------------------------------------

  // Adds the specified |duration| to the timer identified by |id|.  Note that
  // this method is a no-op if there is no existing timer identified by |id|.
  virtual void AddTimeToTimer(const std::string& id,
                              base::TimeDelta duration) = 0;

  // Pauses the timer specified by |id|. Note that this method is a no-op if
  // there is no existing timer identified by |id| or if a timer does exist but
  // is already paused.
  virtual void PauseTimer(const std::string& id) = 0;

  // Remove the alarm/timer specified by |id|.
  // NOTE: this is a no-op if no such alarm/timer exists.
  virtual void RemoveAlarmOrTimer(const std::string& id) = 0;

  // Resumes the timer specified by |id|. Note that this method is a no-op if
  // there is no existing timer identified by |id| or if a timer does exist but
  // isn't currently paused.
  virtual void ResumeTimer(const std::string& id) = 0;
};

using ScopedAssistantInteractionSubscriber =
    base::ScopedObservation<Assistant, AssistantInteractionSubscriber>;

// Main interface between browser and |ash::assistant::Service|.
class COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) AssistantService {
 public:
  AssistantService();
  AssistantService(const AssistantService&) = delete;
  AssistantService& operator=(const AssistantService&) = delete;
  virtual ~AssistantService();

  static AssistantService* Get();

  // Initiates assistant service.
  virtual void Init() = 0;

  // Signals system shutdown, the service could start cleaning up if needed.
  virtual void Shutdown() = 0;

  // Get the main Assistant interface.
  virtual Assistant* GetAssistant() = 0;
};

}  // namespace ash::assistant

// TODO(b/258750971): remove when internal assistant codes are migrated to
// namespace ash.
namespace chromeos::assistant {
using ::ash::assistant::Assistant;
using ::ash::assistant::AssistantInteractionSubscriber;
using ::ash::assistant::AssistantService;
}  // namespace chromeos::assistant

namespace base {

template <>
struct ScopedObservationTraits<ash::assistant::Assistant,
                               ash::assistant::AssistantInteractionSubscriber> {
  static void AddObserver(
      ash::assistant::Assistant* source,
      ash::assistant::AssistantInteractionSubscriber* observer) {
    source->AddAssistantInteractionSubscriber(observer);
  }
  static void RemoveObserver(
      ash::assistant::Assistant* source,
      ash::assistant::AssistantInteractionSubscriber* observer) {
    source->RemoveAssistantInteractionSubscriber(observer);
  }
};

}  // namespace base

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_SERVICE_H_
