// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/speech/speech_recognition_recognizer_impl.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/services/speech/soda/proto/soda_api.pb.h"
#include "chrome/services/speech/soda/soda_client.h"
#include "components/soda/constants.h"
#include "google_apis/google_api_keys.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_sample_types.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace speech {

constexpr char kInvalidAudioDataError[] = "Invalid audio data received.";

// static
const char
    SpeechRecognitionRecognizerImpl::kCaptionBubbleVisibleHistogramName[] =
        "Accessibility.LiveCaption.Duration.CaptionBubbleVisible3";

// static
const char
    SpeechRecognitionRecognizerImpl::kCaptionBubbleHiddenHistogramName[] =
        "Accessibility.LiveCaption.Duration.CaptionBubbleHidden3";

constexpr char kLiveCaptionLanguageCountHistogramName[] =
    "Accessibility.LiveCaption.LanguageCount";

namespace {

// Callback executed by the SODA library on a speech recognition event. The
// callback handle is a void pointer to the SpeechRecognitionRecognizerImpl that
// owns the SODA instance. SpeechRecognitionRecognizerImpl owns the SodaClient
// which owns the instance of SODA and their sequential destruction order
// ensures that this callback will never be called with an invalid callback
// handle to the SpeechRecognitionRecognizerImpl.
void OnSodaResponse(const char* serialized_proto,
                    int length,
                    void* callback_handle) {
  DCHECK(callback_handle);
  soda::chrome::SodaResponse response;
  if (!response.ParseFromArray(serialized_proto, length)) {
    LOG(ERROR) << "Unable to parse result from SODA.";
    return;
  }

  if (response.soda_type() == soda::chrome::SodaResponse::RECOGNITION) {
    soda::chrome::SodaRecognitionResult result = response.recognition_result();
    DCHECK(result.hypothesis_size());
    static_cast<SpeechRecognitionRecognizerImpl*>(callback_handle)
        ->recognition_event_callback()
        .Run(media::SpeechRecognitionResult(
            result.hypothesis(0),
            result.result_type() ==
                soda::chrome::SodaRecognitionResult::FINAL));
  }

  if (response.soda_type() == soda::chrome::SodaResponse::LANGID) {
    // TODO(crbug.com/40167928): Use the langid event to prompt users to switch
    // languages.
    soda::chrome::SodaLangIdEvent event = response.langid_event();

    if (event.confidence_level() >
            static_cast<int>(media::mojom::ConfidenceLevel::kHighlyConfident) ||
        event.confidence_level() <
            static_cast<int>(media::mojom::ConfidenceLevel::kUnknown)) {
      LOG(ERROR) << "Invalid confidence level returned by SODA: "
                 << event.confidence_level();
      return;
    }

    static_cast<SpeechRecognitionRecognizerImpl*>(callback_handle)
        ->language_identification_event_callback()
        .Run(std::string(event.language()),
             static_cast<media::mojom::ConfidenceLevel>(
                 event.confidence_level()),
             static_cast<media::mojom::AsrSwitchResult>(
                 event.asr_switch_result()));
  }

  if (response.soda_type() == soda::chrome::SodaResponse::STOP) {
    static_cast<SpeechRecognitionRecognizerImpl*>(callback_handle)
        ->speech_recognition_stopped_callback()
        .Run();
  }
}

speech::soda::chrome::ExtendedSodaConfigMsg::RecognitionMode
GetSodaSpeechRecognitionMode(
    media::mojom::SpeechRecognitionMode recognition_mode) {
  switch (recognition_mode) {
    case media::mojom::SpeechRecognitionMode::kUnknown:
      return soda::chrome::ExtendedSodaConfigMsg::UNKNOWN;
    case media::mojom::SpeechRecognitionMode::kIme:
      return soda::chrome::ExtendedSodaConfigMsg::IME;
    case media::mojom::SpeechRecognitionMode::kCaption:
      return soda::chrome::ExtendedSodaConfigMsg::CAPTION;
  }
}

}  // namespace

SpeechRecognitionRecognizerImpl::~SpeechRecognitionRecognizerImpl() {
  base::UmaHistogramBoolean(
      base::StrCat({"Accessibility.LiveCaption.", primary_language_name_,
                    ".SessionContainsRecognizedSpeech"}),
      session_contains_speech_);
  RecordDuration();
  soda_client_.reset();

  if (speech_recognition_service_) {
    speech_recognition_service_->RemoveObserver(this);
  }
}

void SpeechRecognitionRecognizerImpl::OnLanguagePackInstalled(
    base::flat_map<std::string, base::FilePath> config_paths) {
  config_paths_ = config_paths;
  ResetSoda();
}

void SpeechRecognitionRecognizerImpl::Create(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> remote,
    media::mojom::SpeechRecognitionOptionsPtr options,
    const base::FilePath& binary_path,
    const base::flat_map<std::string, base::FilePath>& config_paths,
    const std::string& primary_language_name,
    const bool mask_offensive_words,
    base::WeakPtr<SpeechRecognitionServiceImpl> speech_recognition_service) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<SpeechRecognitionRecognizerImpl>(
          std::move(remote), std::move(options), binary_path, config_paths,
          primary_language_name, mask_offensive_words,
          speech_recognition_service),
      std::move(receiver));
}

bool SpeechRecognitionRecognizerImpl::IsMultichannelSupported() {
  return false;
}

void SpeechRecognitionRecognizerImpl::OnRecognitionEvent(
    media::SpeechRecognitionResult event) {
  if (!event.transcription.empty()) {
    session_contains_speech_ = true;
  }

  if (!client_remote_.is_bound())
    return;

  client_remote_->OnSpeechRecognitionRecognitionEvent(
      std::move(event),
      base::BindOnce(&SpeechRecognitionRecognizerImpl::
                         OnSpeechRecognitionRecognitionEventCallback,
                     weak_factory_.GetWeakPtr()));
}

void SpeechRecognitionRecognizerImpl::
    OnSpeechRecognitionRecognitionEventCallback(bool success) {
  is_client_requesting_speech_recognition_ = success;
}

void SpeechRecognitionRecognizerImpl::OnLanguageIdentificationEvent(
    const std::string& language,
    const media::mojom::ConfidenceLevel confidence_level,
    const media::mojom::AsrSwitchResult asr_switch_result) {
  if (client_remote_.is_bound()) {
    client_remote_->OnLanguageIdentificationEvent(
        media::mojom::LanguageIdentificationEvent::New(
            language, confidence_level, asr_switch_result));
  }
}

void SpeechRecognitionRecognizerImpl::OnRecognitionStoppedCallback() {
  if (client_remote_.is_bound()) {
    client_remote_->OnSpeechRecognitionStopped();
  }
}

SpeechRecognitionRecognizerImpl::SpeechRecognitionRecognizerImpl(
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> remote,
    media::mojom::SpeechRecognitionOptionsPtr options,
    const base::FilePath& binary_path,
    const base::flat_map<std::string, base::FilePath>& config_paths,
    const std::string& primary_language_name,
    const bool mask_offensive_words,
    base::WeakPtr<SpeechRecognitionServiceImpl> speech_recognition_service)
    : options_(std::move(options)),
      client_remote_(std::move(remote)),
      config_paths_(config_paths),
      primary_language_name_(primary_language_name),
      mask_offensive_words_(mask_offensive_words),
      speech_recognition_service_(speech_recognition_service) {
  recognition_event_callback_ = base::BindPostTaskToCurrentDefault(
      base::BindRepeating(&SpeechRecognitionRecognizerImpl::OnRecognitionEvent,
                          weak_factory_.GetWeakPtr()));
  language_identification_event_callback_ =
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &SpeechRecognitionRecognizerImpl::OnLanguageIdentificationEvent,
          weak_factory_.GetWeakPtr()));
  speech_recognition_stopped_callback_ =
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &SpeechRecognitionRecognizerImpl::OnRecognitionStoppedCallback,
          weak_factory_.GetWeakPtr()));

  client_remote_.set_disconnect_handler(
      base::BindOnce(&SpeechRecognitionRecognizerImpl::OnClientHostDisconnected,
                     weak_factory_.GetWeakPtr()));

  if (speech_recognition_service_) {
    speech_recognition_service_->AddObserver(this);
  }

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // On Chrome OS Ash, soda_client_ is not used, so don't try to create it
  // here because it exists at a different location. Instead,
  // CrosSpeechRecognitionRecognizerImpl has its own CrosSodaClient.
  DCHECK(base::PathExists(binary_path));
  soda_client_ = std::make_unique<::soda::SodaClient>(binary_path);
  if (!soda_client_->BinaryLoadedSuccessfully()) {
    OnSpeechRecognitionError();
  }
#endif
}

void SpeechRecognitionRecognizerImpl::OnClientHostDisconnected() {
  is_client_requesting_speech_recognition_ = false;
}

void SpeechRecognitionRecognizerImpl::SendAudioToSpeechRecognitionService(
    media::mojom::AudioDataS16Ptr buffer) {
  int channel_count = buffer->channel_count;
  int frame_count = buffer->frame_count;
  int sample_rate = buffer->sample_rate;
  size_t num_samples = 0;
  size_t buffer_size = 0;

  // Update watch time durations.
  if (options_->recognizer_client_type ==
      media::mojom::RecognizerClientType::kLiveCaption) {
    base::TimeDelta duration =
        media::AudioTimestampHelper::FramesToTime(frame_count, sample_rate);
    if (is_client_requesting_speech_recognition_) {
      caption_bubble_visible_duration_ += duration;
    } else {
      caption_bubble_hidden_duration_ += duration;
      return;
    }
  }

  // Verify the channel count.
  if (channel_count <= 0 || channel_count > media::limits::kMaxChannels) {
    mojo::ReportBadMessage(kInvalidAudioDataError);
    return;
  }

  // Verify and calculate the number of samples.
  if (sample_rate <= 0 || frame_count <= 0 ||
      !base::CheckMul(frame_count, channel_count).AssignIfValid(&num_samples) ||
      num_samples != buffer->data.size()) {
    mojo::ReportBadMessage(kInvalidAudioDataError);
    return;
  }

  // Verify and calculate the buffer size.
  if (!base::CheckMul(buffer->data.size(), sizeof(buffer->data[0]))
           .AssignIfValid(&buffer_size)) {
    mojo::ReportBadMessage(kInvalidAudioDataError);
    return;
  }

  // Skip this buffer if there has been no nonzero data for several seconds.
  if (options_->skip_continuously_empty_audio) {
    const bool buffer_is_zero =
        std::all_of(buffer->data.begin(), buffer->data.end(),
                    [](int16_t x) { return x == 0; });
    const base::Time now = base::Time::Now();
    if (!buffer_is_zero) {
      last_non_empty_audio_time_ = now;
    }
    if (now - last_non_empty_audio_time_ > base::Seconds(10)) {
      // No nonzero data for several seconds. Don't send this buffer of zeroes.
      return;
    }
  }

  // OK, everything is verified, let's send the audio.
  SendAudioToSpeechRecognitionServiceInternal(std::move(buffer));
}

void SpeechRecognitionRecognizerImpl::OnSpeechRecognitionError() {
  if (client_remote_.is_bound()) {
    client_remote_->OnSpeechRecognitionError();
  }
}

void SpeechRecognitionRecognizerImpl::MarkDone() {
  soda_client_->MarkDone();
}

void SpeechRecognitionRecognizerImpl::AddAudio(
    media::mojom::AudioDataS16Ptr buffer) {
  SendAudioToSpeechRecognitionService(std::move(buffer));
}
void SpeechRecognitionRecognizerImpl::OnAudioCaptureEnd() {
  MarkDone();
}
void SpeechRecognitionRecognizerImpl::OnAudioCaptureError() {
  OnSpeechRecognitionError();
}

void SpeechRecognitionRecognizerImpl::
    SendAudioToSpeechRecognitionServiceInternal(
        media::mojom::AudioDataS16Ptr buffer) {
  channel_count_ = buffer->channel_count;
  sample_rate_ = buffer->sample_rate;
  size_t buffer_size = 0;
  // Verify and calculate the buffer size.
  if (!base::CheckMul(buffer->data.size(), sizeof(buffer->data[0]))
           .AssignIfValid(&buffer_size)) {
    mojo::ReportBadMessage(kInvalidAudioDataError);
    return;
  }

  CHECK(soda_client_);
  if (!soda_client_->IsInitialized() ||
      soda_client_->DidAudioPropertyChange(sample_rate_, channel_count_)) {
    ResetSoda();
  }

  soda_client_->AddAudio(reinterpret_cast<char*>(buffer->data.data()),
                         buffer_size);
}

void SpeechRecognitionRecognizerImpl::OnLanguageChanged(
    const std::string& language) {
  std::optional<speech::SodaLanguagePackComponentConfig>
      language_component_config = GetLanguageComponentConfig(language);
  if (!language_component_config.has_value())
    return;

  // Only reset SODA if the language changed.
  if (language_component_config.value().language_name ==
          primary_language_name_ ||
      language_component_config.value().language_code == LanguageCode::kNone) {
    return;
  }

  if (!task_runner_) {
    task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  }

  // Changing the language requires a blocking call to check if the language
  // pack exists on the device.
  scoped_refptr<base::SequencedTaskRunner> current_task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](const std::string& language) {
            base::FilePath config_file_path =
                GetLatestSodaLanguagePackDirectory(language);
            return std::make_pair(config_file_path,
                                  base::PathExists(config_file_path));
          },
          language),
      base::BindOnce(&SpeechRecognitionRecognizerImpl::ResetSodaWithNewLanguage,
                     weak_factory_.GetWeakPtr(),
                     language_component_config.value().language_name));
}

void SpeechRecognitionRecognizerImpl::OnMaskOffensiveWordsChanged(
    bool mask_offensive_words) {
  mask_offensive_words_ = mask_offensive_words;
  ResetSoda();
}

void SpeechRecognitionRecognizerImpl::ResetSodaWithNewLanguage(
    std::string language_name,
    std::pair<base::FilePath, bool> config_and_exists) {
  if (config_and_exists.second) {
    config_paths_[language_name] = config_and_exists.first;
    primary_language_name_ = language_name;
    ResetSoda();
  }
}

void SpeechRecognitionRecognizerImpl::RecordDuration() {
  if (options_->recognizer_client_type !=
      media::mojom::RecognizerClientType::kLiveCaption) {
    return;
  }

  // TODO(b:245620092) Create metrics for other features using speech
  // recognition.
  if (caption_bubble_visible_duration_.is_positive()) {
    base::UmaHistogramLongTimes100(kCaptionBubbleVisibleHistogramName,
                                   caption_bubble_visible_duration_);
  }

  if (caption_bubble_hidden_duration_.is_positive()) {
    base::UmaHistogramLongTimes100(kCaptionBubbleHiddenHistogramName,
                                   caption_bubble_hidden_duration_);
  }
}

void SpeechRecognitionRecognizerImpl::ResetSoda() {
  // Initialize the SODA instance.
  auto api_key = google_apis::GetSodaAPIKey();

  // TODO(crbug.com/40162502): Use language from SpeechRecognitionOptions
  // to determine the appropriate language pack path. Note that
  // SodaInstaller::GetLanguagePath() is not implemented outside of Chrome OS,
  // and options_->language is not set for Live Caption.
  std::string language_pack_directory =
      config_paths_[primary_language_name_].AsUTF8Unsafe();

  // Initialize the SODA instance with the serialized config.
  soda::chrome::ExtendedSodaConfigMsg config_msg;
  config_msg.set_channel_count(channel_count_);
  config_msg.set_sample_rate(sample_rate_);
  config_msg.set_api_key(api_key);
  config_msg.set_language_pack_directory(language_pack_directory);
  config_msg.set_simulate_realtime_testonly(false);
  config_msg.set_enable_lang_id(false);
  config_msg.set_recognition_mode(
      GetSodaSpeechRecognitionMode(options_->recognition_mode));
  config_msg.set_enable_formatting(options_->enable_formatting);
  config_msg.set_enable_speaker_change_detection(
      base::FeatureList::IsEnabled(media::kSpeakerChangeDetection));
  config_msg.set_mask_offensive_words(mask_offensive_words_);
  if (base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage) &&
      config_paths_.size() > 0) {
    auto* multilang_config = config_msg.mutable_multilang_config();
    multilang_config->set_rewind_when_switching_language(true);
    auto& multilang_language_pack_directory =
        *(multilang_config->mutable_multilang_language_pack_directory());
    for (const auto& config : config_paths_) {
      multilang_language_pack_directory[base::ToLowerASCII(config.first)] =
          config.second.AsUTF8Unsafe();
    }

    base::UmaHistogramCounts100(kLiveCaptionLanguageCountHistogramName,
                                config_paths_.size());
  }

  auto serialized = config_msg.SerializeAsString();

  SerializedSodaConfig config;
  config.soda_config = serialized.c_str();
  config.soda_config_size = serialized.size();
  config.callback = &OnSodaResponse;
  config.callback_handle = this;
  CHECK(soda_client_);
  soda_client_->Reset(config, sample_rate_, channel_count_);
}

}  // namespace speech
