// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/services/speech/soda/cros_soda_client.h"
#include "base/run_loop.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace {

media::SpeechRecognitionResult GetSpeechRecognitionResultFromFinalEvent(
    const chromeos::machine_learning::mojom::FinalResultPtr& final_event) {
  media::SpeechRecognitionResult result;
  result.transcription = final_event->final_hypotheses.front();
  result.is_final = true;

  if (!final_event->timing_event || !final_event->hypothesis_part)
    return result;

  const auto& timing_event = final_event->timing_event;
  media::TimingInformation timing;
  timing.audio_start_time = timing_event->audio_start_time;
  timing.audio_end_time = timing_event->event_end_time;
  timing.hypothesis_parts = std::vector<media::HypothesisParts>();

  for (const auto& part : final_event->hypothesis_part.value())
    timing.hypothesis_parts->emplace_back(part->text, part->alignment);

  result.timing_information = timing;

  return result;
}

}  // namespace
namespace soda {
CrosSodaClient::CrosSodaClient() : soda_client_(this) {}
CrosSodaClient::~CrosSodaClient() = default;

bool CrosSodaClient::DidAudioPropertyChange(int sample_rate,
                                            int channel_count) {
  return !is_initialized_ || sample_rate_ != sample_rate ||
         channel_count_ != channel_count;
}

void CrosSodaClient::AddAudio(const char* audio_buffer,
                              int audio_buffer_size) const {
  DCHECK(IsInitialized()) << "Unable to add audio before starting.";
  const uint8_t* audio_buffer_casted =
      reinterpret_cast<const uint8_t*>(audio_buffer);
  std::vector<uint8_t> audio(audio_buffer_casted,
                             audio_buffer_casted + audio_buffer_size);
  soda_recognizer_->AddAudio(audio);
}

void CrosSodaClient::MarkDone() {
  DCHECK(IsInitialized()) << "Can't mark as done before starting";
  soda_recognizer_->MarkDone();
}

void CrosSodaClient::Reset(
    chromeos::machine_learning::mojom::SodaConfigPtr soda_config,
    CrosSodaClient::TranscriptionResultCallback transcription_callback,
    CrosSodaClient::OnStopCallback stop_callback,
    CrosSodaClient::OnLanguageIdentificationEventCallback langid_callback) {
  sample_rate_ = soda_config->sample_rate;
  channel_count_ = soda_config->channel_count;
  if (is_initialized_) {
    soda_recognizer_->Stop();
  }
  soda_recognizer_.reset();
  soda_client_.reset();
  ml_service_.reset();
  is_initialized_ = true;
  chromeos::machine_learning::ServiceConnection::GetInstance()
      ->BindMachineLearningService(ml_service_.BindNewPipeAndPassReceiver());
  ml_service_->LoadSpeechRecognizer(
      std::move(soda_config), soda_client_.BindNewPipeAndPassRemote(),
      soda_recognizer_.BindNewPipeAndPassReceiver(),
      base::BindOnce(
          [](chromeos::machine_learning::mojom::LoadModelResult result) {
            if (result !=
                chromeos::machine_learning::mojom::LoadModelResult::OK) {
              LOG(DFATAL) << "Could not load recognizer, error: " << result;
            }
          }));

  transcription_callback_ = transcription_callback;
  stop_callback_ = stop_callback;
  langid_callback_ = langid_callback;

  // Ensure this one is started.
  soda_recognizer_->Start();
}

void CrosSodaClient::OnStop() {
  stop_callback_.Run();
}

void CrosSodaClient::OnStart() {
  // Do nothing OnStart.
}
void CrosSodaClient::OnSpeechRecognizerEvent(
    chromeos::machine_learning::mojom::SpeechRecognizerEventPtr event) {
  if (event->is_final_result()) {
    auto& final_result = event->get_final_result();
    if (!final_result->final_hypotheses.empty())
      transcription_callback_.Run(
          GetSpeechRecognitionResultFromFinalEvent(final_result));
  } else if (event->is_partial_result()) {
    auto& partial_result = event->get_partial_result();
    if (!partial_result->partial_text.empty()) {
      const std::string partial_hyp = partial_result->partial_text.front();
      transcription_callback_.Run(
          media::SpeechRecognitionResult(partial_hyp, false));
    }
  } else if (event->is_langid_event()) {
    const auto& langid_event = event->get_langid_event();
    langid_callback_.Run(langid_event->language,
                         static_cast<media::mojom::ConfidenceLevel>(
                             langid_event->confidence_level),
                         static_cast<media::mojom::AsrSwitchResult>(
                             langid_event->asr_switch_result));
  } else if (!event->is_endpointer_event() && !event->is_audio_event()) {
    LOG(ERROR) << "Some kind of other soda event, ignoring completely. Tag is '"
               << static_cast<uint32_t>(event->which()) << "'";
  }
}

}  // namespace soda
