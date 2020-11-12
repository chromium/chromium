// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SPEECH_CLOUD_SPEECH_RECOGNITION_CLIENT_H_
#define CHROME_SERVICES_SPEECH_CLOUD_SPEECH_RECOGNITION_CLIENT_H_

#include "base/callback.h"
#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "base/unguessable_token.h"
#include "chrome/services/speech/speech_recognition_service_impl.h"
#include "components/speech/chunked_byte_buffer.h"
#include "components/speech/downstream_loader.h"
#include "components/speech/downstream_loader_client.h"
#include "components/speech/upstream_loader.h"
#include "components/speech/upstream_loader_client.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace speech {

// Encapsulates the configuration parameters used to initialize the stream.
struct CloudSpeechConfig {
  int sample_rate;
  int channel_count;
  std::string language_code;
};

// Streams audio to the Open Speech API to generate transcriptions. This is a
// temporary solution that will enable testing and experimentation of the Live
// Caption feature while the Speech On-Device API (SODA) is under development.
// Much of this implementation overlaps with that of the SpeechRecognitionEngine
// used by the WebSpeech API. This code is intentionally kept separate from the
// WebSpeech API implementation to reduce code churn once this client is removed
// and replaced with the SodaClient.
class CloudSpeechRecognitionClient : public speech::UpstreamLoaderClient,
                                     public speech::DownstreamLoaderClient {
 public:
  using OnRecognitionEventCallback =
      base::RepeatingCallback<void(const std::string& result,
                                   const bool is_final)>;

  explicit CloudSpeechRecognitionClient(
      OnRecognitionEventCallback callback,
      base::WeakPtr<SpeechRecognitionServiceImpl>
          speech_recognition_service_impl);
  ~CloudSpeechRecognitionClient() override;

  // Checks whether the sample rate or channel count differs from the values
  // used to initialize the stream.
  bool DidAudioPropertyChange(int sample_rate, int channel_count);

  // Initializes the stream instance with the provided config.
  void Initialize(const CloudSpeechConfig& config);

  // speech::DownstreamLoaderClient
  void OnDownstreamDataReceived(base::StringPiece new_response_data) override;
  void OnDownstreamDataComplete(bool success, int response_code) override {}

  // speech::UpstreamLoaderClient
  void OnUpstreamDataComplete(bool success, int response_code) override {}

  // Resets the stream instance.
  void Reset();

  // Feeds raw audio to the Open Speech API.
  void AddAudio(base::span<const char> chunk);

  void SetUrlLoaderFactoryForTesting(
      mojo::PendingRemote<network::mojom::URLLoaderFactory> factory);

  // Returns a flag indicating whether the stream has been initialized.
  bool IsInitialized() { return is_initialized_; }

  OnRecognitionEventCallback recognition_event_callback() {
    return recognition_event_callback_;
  }

 private:
  friend class speech::UpstreamLoader;
  friend class speech::DownstreamLoader;
  void ResetUrlLoaderFactory();

  bool is_initialized_ = false;

  // Used by histogram only.
  bool audio_property_changed_midstream_ = false;

  int sample_rate_ = 0;
  int channel_count_ = 0;
  std::string language_code_;

  std::string previous_result_;

  // Stores the last time the stream was reset.
  base::TimeTicks last_reset_;

  // Stores the last time audio was uploaded.
  base::TimeTicks last_upload_;

  OnRecognitionEventCallback recognition_event_callback_;

  std::unique_ptr<speech::UpstreamLoader> upstream_loader_;
  std::unique_ptr<speech::DownstreamLoader> downstream_loader_;

  // Remote owned by the SpeechRecognitionServiceImpl.
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;
  base::WeakPtr<SpeechRecognitionServiceImpl> speech_recognition_service_impl_;

  ChunkedByteBuffer chunked_byte_buffer_;
};

}  // namespace speech

#endif  // CHROME_SERVICES_SPEECH_CLOUD_SPEECH_RECOGNITION_CLIENT_H_
