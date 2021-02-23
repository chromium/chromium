// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/speech/soda/cros_soda_client.h"
#include "base/run_loop.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

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

void CrosSodaClient::Reset(
    chromeos::machine_learning::mojom::SodaConfigPtr soda_config,
    base::RepeatingCallback<void(const std::string&, bool)> callback) {
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

  callback_ = callback;
  // Ensure this one is started.
  soda_recognizer_->Start();
}
void CrosSodaClient::OnStop() {
  // Do nothing OnStop.
}
void CrosSodaClient::OnStart() {
  // Do nothing OnStart.
}
void CrosSodaClient::OnSpeechRecognizerEvent(
    chromeos::machine_learning::mojom::SpeechRecognizerEventPtr event) {
  if (event->is_final_result()) {
    auto& final_result = event->get_final_result();
    if (!final_result->final_hypotheses.empty()) {
      const std::string final_hyp = final_result->final_hypotheses.front();
      callback_.Run(final_hyp, true);
    }
  } else if (event->is_partial_result()) {
    auto& partial_result = event->get_partial_result();
    if (!partial_result->partial_text.empty()) {
      const std::string partial_hyp = partial_result->partial_text.front();
      callback_.Run(partial_hyp, false);
    }
  } else {
    LOG(ERROR) << "Some kind of other soda event, ignoring completely.";
  }
}

}  // namespace soda
