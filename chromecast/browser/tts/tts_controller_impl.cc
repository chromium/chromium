// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/tts/tts_controller_impl.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chromecast/browser/tts/tts_platform.h"
#include "chromecast/chromecast_buildflags.h"
#include "third_party/blink/public/mojom/speech/speech_synthesis.mojom-forward.h"

#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
#include "extensions/browser/extensions_browser_client.h"
#endif

namespace {
// A value to be used to indicate that there is no char index available.
const int kInvalidCharIndex = -1;

// Given a language/region code of the form 'fr-FR', returns just the basic
// language portion, e.g. 'fr'.
std::string TrimLanguageCode(const std::string& lang) {
  if (lang.size() >= 5 && lang[2] == '-')
    return lang.substr(0, 2);
  else
    return lang;
}

// IMPORTANT!
// These values are written to logs.  Do not renumber or delete
// existing items; add new entries to the end of the list.
enum class UMATextToSpeechEvent {
  START = 0,
  END = 1,
  WORD = 2,
  SENTENCE = 3,
  MARKER = 4,
  INTERRUPTED = 5,
  CANCELLED = 6,
  SPEECH_ERROR = 7,
  PAUSE = 8,
  RESUME = 9,

  // This must always be the last enum. It's okay for its value to
  // increase, but none of the other enum values may change.
  COUNT
};

}  // namespace

bool IsFinalTtsEventType(TtsEventType event_type) {
  return (event_type == TTS_EVENT_END || event_type == TTS_EVENT_INTERRUPTED ||
          event_type == TTS_EVENT_CANCELLED || event_type == TTS_EVENT_ERROR);
}

//
// UtteranceContinuousParameters
//

UtteranceContinuousParameters::UtteranceContinuousParameters()
    : rate(blink::mojom::kSpeechSynthesisDoublePrefNotSet),
      pitch(blink::mojom::kSpeechSynthesisDoublePrefNotSet),
      volume(blink::mojom::kSpeechSynthesisDoublePrefNotSet) {}

//
// VoiceData
//

VoiceData::VoiceData()
    : gender(TTS_GENDER_NONE), remote(false), native(false) {}

VoiceData::VoiceData(const VoiceData& other) = default;

VoiceData::~VoiceData() {}

//
// Utterance
//

// static
int Utterance::next_utterance_id_ = 0;

Utterance::Utterance(content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      id_(next_utterance_id_++),
      src_id_(-1),
      gender_(TTS_GENDER_NONE),
      can_enqueue_(false),
      char_index_(0),
      finished_(false) {
  options_.reset(new base::DictionaryValue());
}

Utterance::~Utterance() {
  // It's an error if an Utterance is destructed without being finished,
  // unless |browser_context_| is nullptr because it's a unit test.
  DCHECK(finished_ || !browser_context_);
}

void Utterance::OnTtsEvent(TtsEventType event_type,
                           int char_index,
                           const std::string& error_message) {
  if (char_index >= 0)
    char_index_ = char_index;
  if (IsFinalTtsEventType(event_type))
    finished_ = true;

  if (event_delegate_)
    event_delegate_->OnTtsEvent(this, event_type, char_index, error_message);
  if (finished_)
    event_delegate_ = nullptr;
}

void Utterance::Finish() {
  finished_ = true;
}

void Utterance::set_options(const base::Value* options) {
  options_.reset(options->DeepCopy());
}

//
// TtsControllerImpl
//

TtsControllerImpl::TtsControllerImpl(
    std::unique_ptr<TtsPlatformImpl> platform_impl)
    : current_utterance_(nullptr),
      paused_(false),
      platform_impl_(std::move(platform_impl)) {}

TtsControllerImpl::~TtsControllerImpl() {
  if (current_utterance_) {
    current_utterance_->Finish();
    delete current_utterance_;
  }

  // Clear any queued utterances too.
  ClearUtteranceQueue(false);  // Don't sent events.
}

void TtsControllerImpl::SpeakOrEnqueue(Utterance* utterance) {
  // If we're paused and we get an utterance that can't be queued,
  // flush the queue but stay in the paused state.
  if (paused_ && !utterance->can_enqueue()) {
    utterance_queue_.push(utterance);
    Stop();
    paused_ = true;
    return;
  }

  if (paused_ || (IsSpeaking() && utterance->can_enqueue())) {
    utterance_queue_.push(utterance);
  } else {
    Stop();
    SpeakNow(utterance);
  }
}

void TtsControllerImpl::SpeakNow(Utterance* utterance) {
  // Get all available voices and try to find a matching voice.
  std::vector<VoiceData> voices;
  GetVoices(utterance->browser_context(), &voices);

  // Get the best matching voice. If nothing matches, just set "native"
  // to true because that might trigger deferred loading of native voices.
  int index = GetMatchingVoice(utterance, voices);
  VoiceData voice;
  if (index >= 0)
    voice = voices[index];
  else
    voice.native = true;

  UpdateUtteranceDefaults(utterance);

  GetPlatformImpl()->WillSpeakUtteranceWithVoice(utterance, voice);

  base::RecordAction(base::UserMetricsAction("TextToSpeech.Speak"));
  UMA_HISTOGRAM_COUNTS_100000("TextToSpeech.Utterance.TextLength",
                              utterance->text().size());
  UMA_HISTOGRAM_BOOLEAN("TextToSpeech.Utterance.FromExtensionAPI",
                        !utterance->src_url().is_empty());
  UMA_HISTOGRAM_BOOLEAN("TextToSpeech.Utterance.HasVoiceName",
                        !utterance->voice_name().empty());
  UMA_HISTOGRAM_BOOLEAN("TextToSpeech.Utterance.HasLang",
                        !utterance->lang().empty());
  UMA_HISTOGRAM_BOOLEAN("TextToSpeech.Utterance.HasGender",
                        utterance->gender() != TTS_GENDER_NONE);
  UMA_HISTOGRAM_BOOLEAN("TextToSpeech.Utterance.HasRate",
                        utterance->continuous_parameters().rate != 1.0);
  UMA_HISTOGRAM_BOOLEAN("TextToSpeech.Utterance.HasPitch",
                        utterance->continuous_parameters().pitch != 1.0);
  UMA_HISTOGRAM_BOOLEAN("TextToSpeech.Utterance.HasVolume",
                        utterance->continuous_parameters().volume != 1.0);
  UMA_HISTOGRAM_BOOLEAN("TextToSpeech.Utterance.Native", voice.native);

  if (!voice.native) {
#if !defined(OS_ANDROID)
    DCHECK(!voice.extension_id.empty());
    current_utterance_ = utterance;
    utterance->set_extension_id(voice.extension_id);
    bool sends_end_event =
        voice.events.find(TTS_EVENT_END) != voice.events.end();
    if (!sends_end_event) {
      utterance->Finish();
      delete utterance;
      current_utterance_ = nullptr;
      SpeakNextUtterance();
    }
#endif
  } else {
    // It's possible for certain platforms to send start events immediately
    // during |speak|.
    current_utterance_ = utterance;
    GetPlatformImpl()->clear_error();
    bool success = GetPlatformImpl()->Speak(utterance->id(), utterance->text(),
                                            utterance->lang(), voice,
                                            utterance->continuous_parameters());
    if (!success) {
      current_utterance_ = nullptr;
      utterance->OnTtsEvent(TTS_EVENT_ERROR, kInvalidCharIndex,
                            GetPlatformImpl()->error());
      delete utterance;
      return;
    }
  }
}

void TtsControllerImpl::Stop() {
  base::RecordAction(base::UserMetricsAction("TextToSpeech.Stop"));

  paused_ = false;
  GetPlatformImpl()->clear_error();
  GetPlatformImpl()->StopSpeaking();

  if (current_utterance_)
    current_utterance_->OnTtsEvent(TTS_EVENT_INTERRUPTED, kInvalidCharIndex,
                                   std::string());
  FinishCurrentUtterance();
  ClearUtteranceQueue(true);  // Send events.
}

void TtsControllerImpl::Pause() {
  base::RecordAction(base::UserMetricsAction("TextToSpeech.Pause"));

  paused_ = true;
  if (current_utterance_) {
    GetPlatformImpl()->clear_error();
    GetPlatformImpl()->Pause();
  }
}

void TtsControllerImpl::Resume() {
  base::RecordAction(base::UserMetricsAction("TextToSpeech.Resume"));

  paused_ = false;
  if (current_utterance_) {
    GetPlatformImpl()->clear_error();
    GetPlatformImpl()->Resume();
  } else {
    SpeakNextUtterance();
  }
}

void TtsControllerImpl::OnTtsEvent(int utterance_id,
                                   TtsEventType event_type,
                                   int char_index,
                                   const std::string& error_message) {
  // We may sometimes receive completion callbacks "late", after we've
  // already finished the utterance (for example because another utterance
  // interrupted or we got a call to Stop). This is normal and we can
  // safely just ignore these events.
  if (!current_utterance_ || utterance_id != current_utterance_->id()) {
    return;
  }

  UMATextToSpeechEvent metric;
  switch (event_type) {
    case TTS_EVENT_START:
      metric = UMATextToSpeechEvent::START;
      break;
    case TTS_EVENT_END:
      metric = UMATextToSpeechEvent::END;
      break;
    case TTS_EVENT_WORD:
      metric = UMATextToSpeechEvent::WORD;
      break;
    case TTS_EVENT_SENTENCE:
      metric = UMATextToSpeechEvent::SENTENCE;
      break;
    case TTS_EVENT_MARKER:
      metric = UMATextToSpeechEvent::MARKER;
      break;
    case TTS_EVENT_INTERRUPTED:
      metric = UMATextToSpeechEvent::INTERRUPTED;
      break;
    case TTS_EVENT_CANCELLED:
      metric = UMATextToSpeechEvent::CANCELLED;
      break;
    case TTS_EVENT_ERROR:
      metric = UMATextToSpeechEvent::SPEECH_ERROR;
      break;
    case TTS_EVENT_PAUSE:
      metric = UMATextToSpeechEvent::PAUSE;
      break;
    case TTS_EVENT_RESUME:
      metric = UMATextToSpeechEvent::RESUME;
      break;
    default:
      NOTREACHED();
      return;
  }
  UMA_HISTOGRAM_ENUMERATION("TextToSpeech.Event", metric,
                            UMATextToSpeechEvent::COUNT);

  current_utterance_->OnTtsEvent(event_type, char_index, error_message);
  if (current_utterance_->finished()) {
    FinishCurrentUtterance();
    SpeakNextUtterance();
  }
}

void TtsControllerImpl::GetVoices(content::BrowserContext* browser_context,
                                  std::vector<VoiceData>* out_voices) {
  TtsPlatformImpl* platform_impl = GetPlatformImpl();
  if (platform_impl) {
    // Ensure we have all built-in voices loaded. This is a no-op if already
    // loaded.
    if (platform_impl->PlatformImplAvailable())
      platform_impl->GetVoices(out_voices);
  }
}

bool TtsControllerImpl::IsSpeaking() {
  return current_utterance_ != nullptr || GetPlatformImpl()->IsSpeaking();
}

void TtsControllerImpl::FinishCurrentUtterance() {
  if (current_utterance_) {
    if (!current_utterance_->finished())
      current_utterance_->OnTtsEvent(TTS_EVENT_INTERRUPTED, kInvalidCharIndex,
                                     std::string());
    delete current_utterance_;
    current_utterance_ = nullptr;
  }
}

void TtsControllerImpl::SpeakNextUtterance() {
  if (paused_)
    return;

  // Start speaking the next utterance in the queue.  Keep trying in case
  // one fails but there are still more in the queue to try.
  while (!utterance_queue_.empty() && !current_utterance_) {
    Utterance* utterance = utterance_queue_.front();
    utterance_queue_.pop();
    SpeakNow(utterance);
  }
}

void TtsControllerImpl::ClearUtteranceQueue(bool send_events) {
  while (!utterance_queue_.empty()) {
    Utterance* utterance = utterance_queue_.front();
    utterance_queue_.pop();
    if (send_events)
      utterance->OnTtsEvent(TTS_EVENT_CANCELLED, kInvalidCharIndex,
                            std::string());
    else
      utterance->Finish();
    delete utterance;
  }
}

void TtsControllerImpl::SetPlatformImpl(
    std::unique_ptr<TtsPlatformImpl> platform_impl) {
  platform_impl_ = std::move(platform_impl);
}

int TtsControllerImpl::QueueSize() {
  return static_cast<int>(utterance_queue_.size());
}

TtsPlatformImpl* TtsControllerImpl::GetPlatformImpl() {
  return platform_impl_.get();
}

std::string TtsControllerImpl::GetApplicationLocale() const {
  // TODO(rdaum): Delegate back to the platform's mechaninism here.
  // For Chrome, use g_browser_process as before. For cast, it's the system
  // locale. Hardcoded to system locale for now
#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  return extensions::ExtensionsBrowserClient::Get()->GetApplicationLocale();
#else
  return "en-US";
#endif
}

int TtsControllerImpl::GetMatchingVoice(const Utterance* utterance,
                                        std::vector<VoiceData>& voices) {
  // Return the index of the voice that best match the utterance parameters.
  //
  // These criteria are considered mandatory - if they're specified, any voice
  // that doesn't match is rejected.
  //
  //   Extension ID
  //   Voice name
  //
  // The other criteria are scored based on how well they match, in
  // this order of precedence:
  //
  //   Utterange language (exact region preferred, then general language)
  //   App/system language (exact region preferred, then general language)
  //   Required event types
  //   Gender

  std::string app_lang = GetApplicationLocale();

  // Start with a best score of -1, that way even if none of the criteria
  // match, something will be returned if there are any voices.
  int best_score = -1;
  int best_score_index = -1;
  for (size_t i = 0; i < voices.size(); ++i) {
    const VoiceData& voice = voices[i];
    int score = 0;

    // If the extension ID is specified, check for an exact match.
    if (!utterance->extension_id().empty() &&
        utterance->extension_id() != voice.extension_id)
      continue;

    // If the voice name is specified, check for an exact match.
    if (!utterance->voice_name().empty() &&
        voice.name != utterance->voice_name())
      continue;

    // Prefer the utterance language.
    if (!voice.lang.empty() && !utterance->lang().empty()) {
      // An exact language match is worth more than a partial match.
      if (voice.lang == utterance->lang()) {
        score += 32;
      } else if (TrimLanguageCode(voice.lang) ==
                 TrimLanguageCode(utterance->lang())) {
        score += 16;
      }
    }

    // Prefer the system language after that.
    if (!voice.lang.empty()) {
      if (voice.lang == app_lang)
        score += 8;
      else if (TrimLanguageCode(voice.lang) == TrimLanguageCode(app_lang))
        score += 4;
    }

    // Next, prefer required event types.
    if (utterance->required_event_types().size() > 0) {
      bool has_all_required_event_types = true;
      for (std::set<TtsEventType>::const_iterator iter =
               utterance->required_event_types().begin();
           iter != utterance->required_event_types().end(); ++iter) {
        if (voice.events.find(*iter) == voice.events.end()) {
          has_all_required_event_types = false;
          break;
        }
      }
      if (has_all_required_event_types)
        score += 2;
    }

    // Finally prefer the requested gender last.
    if (voice.gender != TTS_GENDER_NONE &&
        utterance->gender() != TTS_GENDER_NONE &&
        voice.gender == utterance->gender()) {
      score += 1;
    }

    if (score > best_score) {
      best_score = score;
      best_score_index = i;
    }
  }

  return best_score_index;
}

void TtsControllerImpl::UpdateUtteranceDefaults(Utterance* utterance) {
  double rate = utterance->continuous_parameters().rate;
  double pitch = utterance->continuous_parameters().pitch;
  double volume = utterance->continuous_parameters().volume;
  // Update pitch, rate and volume to defaults if not explicity set on
  // this utterance.
  if (rate == blink::mojom::kSpeechSynthesisDoublePrefNotSet)
    rate = blink::mojom::kSpeechSynthesisDefaultRate;
  if (pitch == blink::mojom::kSpeechSynthesisDoublePrefNotSet)
    pitch = blink::mojom::kSpeechSynthesisDefaultPitch;
  if (volume == blink::mojom::kSpeechSynthesisDoublePrefNotSet)
    volume = blink::mojom::kSpeechSynthesisDefaultVolume;
  utterance->set_continuous_parameters(rate, pitch, volume);
}
