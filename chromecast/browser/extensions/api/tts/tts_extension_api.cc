// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// PLEASE NOTE: this is a copy with modifications from
// /chrome/browser/speech/extension_api
// It is temporary until a refactoring to move the chrome TTS implementation up
// into components and extensions/components can be completed.

#include "chromecast/browser/extensions/api/tts/tts_extension_api.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include "base/lazy_instance.h"
#include "base/values.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/extensions/api/tts/tts_extension_api_constants.h"
#include "content/public/browser/tts_controller.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function_registry.h"
#include "third_party/blink/public/mojom/speech/speech_synthesis.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace constants = tts_extension_api_constants;

namespace events {
const char kOnEvent[] = "tts.onEvent";
}  // namespace events

const char* TtsEventTypeToString(content::TtsEventType event_type) {
  switch (event_type) {
    case content::TTS_EVENT_START:
      return constants::kEventTypeStart;
    case content::TTS_EVENT_END:
      return constants::kEventTypeEnd;
    case content::TTS_EVENT_WORD:
      return constants::kEventTypeWord;
    case content::TTS_EVENT_SENTENCE:
      return constants::kEventTypeSentence;
    case content::TTS_EVENT_MARKER:
      return constants::kEventTypeMarker;
    case content::TTS_EVENT_INTERRUPTED:
      return constants::kEventTypeInterrupted;
    case content::TTS_EVENT_CANCELLED:
      return constants::kEventTypeCancelled;
    case content::TTS_EVENT_ERROR:
      return constants::kEventTypeError;
    case content::TTS_EVENT_PAUSE:
      return constants::kEventTypePause;
    case content::TTS_EVENT_RESUME:
      return constants::kEventTypeResume;
    default:
      NOTREACHED();
      return constants::kEventTypeError;
  }
}

content::TtsEventType TtsEventTypeFromString(const std::string& str) {
  if (str == constants::kEventTypeStart)
    return content::TTS_EVENT_START;
  if (str == constants::kEventTypeEnd)
    return content::TTS_EVENT_END;
  if (str == constants::kEventTypeWord)
    return content::TTS_EVENT_WORD;
  if (str == constants::kEventTypeSentence)
    return content::TTS_EVENT_SENTENCE;
  if (str == constants::kEventTypeMarker)
    return content::TTS_EVENT_MARKER;
  if (str == constants::kEventTypeInterrupted)
    return content::TTS_EVENT_INTERRUPTED;
  if (str == constants::kEventTypeCancelled)
    return content::TTS_EVENT_CANCELLED;
  if (str == constants::kEventTypeError)
    return content::TTS_EVENT_ERROR;
  if (str == constants::kEventTypePause)
    return content::TTS_EVENT_PAUSE;
  if (str == constants::kEventTypeResume)
    return content::TTS_EVENT_RESUME;

  NOTREACHED();
  return content::TTS_EVENT_ERROR;
}

namespace extensions {

// One of these is constructed for each utterance, and deleted
// when the utterance gets any final event.
class TtsExtensionEventHandler : public content::UtteranceEventDelegate {
 public:
  explicit TtsExtensionEventHandler(const std::string& src_extension_id);

  void OnTtsEvent(content::TtsUtterance* utterance,
                  content::TtsEventType event_type,
                  int char_index,
                  int length,
                  const std::string& error_message) override;

 private:
  // The extension ID of the extension that called speak() and should
  // receive events.
  std::string src_extension_id_;
};

TtsExtensionEventHandler::TtsExtensionEventHandler(
    const std::string& src_extension_id)
    : src_extension_id_(src_extension_id) {}

void TtsExtensionEventHandler::OnTtsEvent(content::TtsUtterance* utterance,
                                          content::TtsEventType event_type,
                                          int char_index,
                                          int length,
                                          const std::string& error_message) {
  if (utterance->GetSrcId() < 0) {
    if (utterance->IsFinished())
      delete this;
    return;
  }

  const std::set<content::TtsEventType>& desired_event_types =
      utterance->GetDesiredEventTypes();
  if (!desired_event_types.empty() &&
      desired_event_types.find(event_type) == desired_event_types.end()) {
    if (utterance->IsFinished())
      delete this;
    return;
  }

  const char* event_type_string = TtsEventTypeToString(event_type);
  std::unique_ptr<base::DictionaryValue> details(new base::DictionaryValue());
  if (char_index >= 0)
    details->SetInteger(constants::kCharIndexKey, char_index);
  if (length >= 0)
    details->SetInteger(constants::kLengthKey, length);
  details->SetString(constants::kEventTypeKey, event_type_string);
  if (event_type == content::TTS_EVENT_ERROR) {
    details->SetString(constants::kErrorMessageKey, error_message);
  }
  details->SetInteger(constants::kSrcIdKey, utterance->GetSrcId());
  details->SetBoolean(constants::kIsFinalEventKey, utterance->IsFinished());

  std::unique_ptr<base::ListValue> arguments(new base::ListValue());
  arguments->Append(std::move(details));

  auto event = std::make_unique<extensions::Event>(
      ::extensions::events::TTS_ON_EVENT, ::events::kOnEvent,
      std::move(*arguments).TakeListDeprecated(),
      utterance->GetBrowserContext());
  event->event_url = utterance->GetSrcUrl();
  extensions::EventRouter::Get(utterance->GetBrowserContext())
      ->DispatchEventToExtension(src_extension_id_, std::move(event));

  if (utterance->IsFinished())
    delete this;
}

ExtensionFunction::ResponseAction TtsSpeakFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());
  const std::string& text = args()[0].GetString();
  if (text.size() > 32768) {
    return RespondNow(Error(constants::kErrorUtteranceTooLong));
  }

  std::unique_ptr<base::DictionaryValue> options(new base::DictionaryValue());
  if (args().size() >= 2 && args()[1].is_dict()) {
    const base::DictionaryValue& temp_options =
        base::Value::AsDictionaryValue(args()[1]);
    options.reset(temp_options.DeepCopy());
  }

  std::string voice_name;
  if (options->HasKey(constants::kVoiceNameKey)) {
    EXTENSION_FUNCTION_VALIDATE(
        options->GetString(constants::kVoiceNameKey, &voice_name));
  }

  std::string lang;
  if (options->HasKey(constants::kLangKey))
    EXTENSION_FUNCTION_VALIDATE(options->GetString(constants::kLangKey, &lang));
  if (!lang.empty() && !l10n_util::IsValidLocaleSyntax(lang)) {
    return RespondNow(Error(constants::kErrorInvalidLang));
  }

  double rate = blink::mojom::kSpeechSynthesisDoublePrefNotSet;
  if (options->HasKey(constants::kRateKey)) {
    absl::optional<double> rate_option =
        options->FindDoubleKey(constants::kRateKey);
    EXTENSION_FUNCTION_VALIDATE(rate_option);
    if (rate_option)
      rate = *rate_option;
    if (rate < 0.1 || rate > 10.0) {
      return RespondNow(Error(constants::kErrorInvalidRate));
    }
  }

  double pitch = blink::mojom::kSpeechSynthesisDoublePrefNotSet;
  if (options->HasKey(constants::kPitchKey)) {
    absl::optional<double> pitch_option =
        options->FindDoubleKey(constants::kPitchKey);
    EXTENSION_FUNCTION_VALIDATE(pitch_option);
    if (pitch_option)
      pitch = *pitch_option;
    if (pitch < 0.0 || pitch > 2.0) {
      return RespondNow(Error(constants::kErrorInvalidPitch));
    }
  }

  double volume = blink::mojom::kSpeechSynthesisDoublePrefNotSet;
  if (options->HasKey(constants::kVolumeKey)) {
    absl::optional<double> volume_option =
        options->FindDoubleKey(constants::kVolumeKey);
    EXTENSION_FUNCTION_VALIDATE(volume_option);
    if (volume_option)
      volume = *volume_option;
    if (volume < 0.0 || volume > 1.0) {
      return RespondNow(Error(constants::kErrorInvalidVolume));
    }
  }

  bool can_enqueue =
      options->FindBoolKey(constants::kEnqueueKey).value_or(false);
  if (base::Value* value = options->FindKey(constants::kEnqueueKey)) {
    EXTENSION_FUNCTION_VALIDATE(value->is_bool());
  }

  std::set<content::TtsEventType> required_event_types;
  if (options->HasKey(constants::kRequiredEventTypesKey)) {
    const base::Value* list =
        options->FindListKey(constants::kRequiredEventTypesKey);
    EXTENSION_FUNCTION_VALIDATE(list);
    for (const base::Value& i : list->GetListDeprecated()) {
      const std::string* event_type = i.GetIfString();
      if (event_type) {
        required_event_types.insert(
            TtsEventTypeFromString(event_type->c_str()));
      }
    }
  }

  std::set<content::TtsEventType> desired_event_types;
  if (options->HasKey(constants::kDesiredEventTypesKey)) {
    const base::Value* list =
        options->FindListKey(constants::kDesiredEventTypesKey);
    EXTENSION_FUNCTION_VALIDATE(list);
    for (const base::Value& i : list->GetListDeprecated()) {
      const std::string* event_type = i.GetIfString();
      if (event_type)
        desired_event_types.insert(TtsEventTypeFromString(event_type->c_str()));
    }
  }

  std::string voice_extension_id;
  if (options->HasKey(constants::kExtensionIdKey)) {
    EXTENSION_FUNCTION_VALIDATE(
        options->GetString(constants::kExtensionIdKey, &voice_extension_id));
  }

  int src_id = -1;
  if (options->HasKey(constants::kSrcIdKey)) {
    EXTENSION_FUNCTION_VALIDATE(
        options->GetInteger(constants::kSrcIdKey, &src_id));
  }

  // If we got this far, the arguments were all in the valid format, so
  // send the success response to the callback now - this ensures that
  // the callback response always arrives before events, which makes
  // the behavior more predictable and easier to write unit tests for too.
  Respond(NoArguments());

  std::unique_ptr<content::TtsUtterance> utterance =
      content::TtsUtterance::Create(browser_context());
  utterance->SetText(text);
  utterance->SetVoiceName(voice_name);
  utterance->SetSrcId(src_id);
  utterance->SetSrcUrl(source_url());
  utterance->SetLang(lang);
  utterance->SetContinuousParameters(rate, pitch, volume);
  utterance->SetShouldClearQueue(!can_enqueue);
  utterance->SetRequiredEventTypes(required_event_types);
  utterance->SetDesiredEventTypes(desired_event_types);
  utterance->SetEngineId(voice_extension_id);
  utterance->SetOptions(options.get());
  utterance->SetEventDelegate(new TtsExtensionEventHandler(extension_id()));

  content::TtsController* controller = content::TtsController::GetInstance();
  controller->SpeakOrEnqueue(std::move(utterance));
  return AlreadyResponded();
}

ExtensionFunction::ResponseAction TtsStopSpeakingFunction::Run() {
  content::TtsController::GetInstance()->Stop(source_url());
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction TtsPauseFunction::Run() {
  content::TtsController::GetInstance()->Pause();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction TtsResumeFunction::Run() {
  content::TtsController::GetInstance()->Resume();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction TtsIsSpeakingFunction::Run() {
  return RespondNow(OneArgument(
      base::Value(content::TtsController::GetInstance()->IsSpeaking())));
}

ExtensionFunction::ResponseAction TtsGetVoicesFunction::Run() {
  std::vector<content::VoiceData> voices;
  content::TtsController::GetInstance()->GetVoices(browser_context(), GURL(),
                                                   &voices);

  auto result_voices = std::make_unique<base::ListValue>();
  for (size_t i = 0; i < voices.size(); ++i) {
    const content::VoiceData& voice = voices[i];
    std::unique_ptr<base::DictionaryValue> result_voice(
        new base::DictionaryValue());
    result_voice->SetString(constants::kVoiceNameKey, voice.name);
    result_voice->SetBoolean(constants::kRemoteKey, voice.remote);
    if (!voice.lang.empty())
      result_voice->SetString(constants::kLangKey, voice.lang);
    if (!voice.engine_id.empty())
      result_voice->SetString(constants::kExtensionIdKey, voice.engine_id);

    auto event_types = std::make_unique<base::ListValue>();
    for (auto iter = voice.events.begin(); iter != voice.events.end(); ++iter) {
      const char* event_name_constant = TtsEventTypeToString(*iter);
      event_types->Append(event_name_constant);
    }
    result_voice->Set(constants::kEventTypesKey, std::move(event_types));

    result_voices->Append(std::move(result_voice));
  }

  return RespondNow(
      OneArgument(base::Value::FromUniquePtrValue(std::move(result_voices))));
}

TtsAPI::TtsAPI(content::BrowserContext* context) {
  ExtensionFunctionRegistry& registry =
      ExtensionFunctionRegistry::GetInstance();
  registry.RegisterFunction<TtsGetVoicesFunction>();
  registry.RegisterFunction<TtsIsSpeakingFunction>();
  registry.RegisterFunction<TtsSpeakFunction>();
  registry.RegisterFunction<TtsStopSpeakingFunction>();
  registry.RegisterFunction<TtsPauseFunction>();
  registry.RegisterFunction<TtsResumeFunction>();
}

TtsAPI::~TtsAPI() {}

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<TtsAPI>>::DestructorAtExit g_factory =
    LAZY_INSTANCE_INITIALIZER;

BrowserContextKeyedAPIFactory<TtsAPI>* TtsAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

}  // namespace extensions
