// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/speech/cloud_speech_recognition_client.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "content/public/browser/google_streaming_api.pb.h"
#include "google_apis/google_api_keys.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/base/escape.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"
#include "services/network/public/mojom/chunked_data_pipe_getter.mojom.h"
#include "url/gurl.h"

namespace speech {

// The maximum duration a stream can be open for. The Open Speech API supports 5
// minutes of continuous recognition.
constexpr base::TimeDelta kMaximumStreamDuration =
    base::TimeDelta::FromSeconds(295);

// The Open Speech API will not return any recognition events if 30 seconds have
// elapsed since the last audio upload.
constexpr base::TimeDelta kMaximumPauseDuration =
    base::TimeDelta::FromSeconds(28);

constexpr char kWebServiceBaseUrl[] =
    "https://www.google.com/speech-api/full-duplex/v1";
constexpr char kDownstreamUrl[] = "/down";
constexpr char kUpstreamUrl[] = "/up";

CloudSpeechRecognitionClient::CloudSpeechRecognitionClient(
    OnRecognitionEventCallback callback,
    base::WeakPtr<SpeechRecognitionServiceImpl> speech_recognition_service_impl)
    : recognition_event_callback_(callback),
      speech_recognition_service_impl_(
          std::move(speech_recognition_service_impl)) {
  ResetUrlLoaderFactory();
}

CloudSpeechRecognitionClient::~CloudSpeechRecognitionClient() {
  base::UmaHistogramBoolean("Accessibility.LiveCaption.AudioPropertyChanged",
                            audio_property_changed_midstream_);
}

bool CloudSpeechRecognitionClient::DidAudioPropertyChange(int sample_rate,
                                                          int channel_count) {
  bool property_changed =
      sample_rate != sample_rate_ || channel_count != channel_count_;
  audio_property_changed_midstream_ |= property_changed;
  return property_changed;
}

void CloudSpeechRecognitionClient::Initialize(const CloudSpeechConfig& config) {
  channel_count_ = config.channel_count;
  sample_rate_ = config.sample_rate;
  language_code_ = config.language_code;
  is_initialized_ = true;
  Reset();
}

void CloudSpeechRecognitionClient::OnDownstreamDataReceived(
    base::StringPiece new_response_data) {
  // The downstream response is organized in chunks, whose size is determined
  // by a 4 bytes prefix, transparently handled by the ChunkedByteBuffer class.
  // Such chunks are sent by the speech recognition webservice over the HTTP
  // downstream channel using HTTP chunked transfer (unrelated to our chunks).
  // This function is called every time an HTTP chunk is received by the
  // url fetcher. However there isn't any particular matching between our
  // protocol chunks and HTTP chunks, in the sense that a single HTTP chunk can
  // contain a portion of one chunk or even more chunks together.
  chunked_byte_buffer_.Append(new_response_data);
  std::string result;
  bool is_final = false;

  // A single HTTP chunk can contain more than one data chunk, thus the while.
  while (chunked_byte_buffer_.HasChunks()) {
    auto chunk = chunked_byte_buffer_.PopChunk();
    content::proto::SpeechRecognitionEvent event;
    if (!event.ParseFromArray(chunk->data(), chunk->size() * sizeof(uint8_t))) {
      DLOG(ERROR) << "Parsing of the recognition response failed.";
      return;
    }

    // A speech recognition event can have multiple recognition results in
    // descending order of stability. Concatenate all of the recognition result
    // parts to build the full transcription.
    for (const auto& recognition_result : event.result()) {
      is_final |= recognition_result.final();
      if (recognition_result.has_stability()) {
        for (const auto& alternative : recognition_result.alternative()) {
          if (alternative.has_transcript())
            result += alternative.transcript();
        }
      }
    }

    // Remove the leading whitespace that the Open Speech API automatically
    // prepends because the captioning bubble will handle the formatting.
    if (!result.empty() && result[0] == ' ')
      result.erase(0, 1);

    // The Open Speech API returns an empty recognition event with |final|
    // marked as true to indicate that the previous result returned was a final
    // recognition result.
    if (is_final && result.empty())
      result = previous_result_;

    previous_result_ = result;
    recognition_event_callback().Run(result, is_final);
  }
}

void CloudSpeechRecognitionClient::Reset() {
  DCHECK(is_initialized_);
  // Return if the URL loader factory has not been set.
  if (!url_loader_factory_)
    return;

  last_reset_ = base::TimeTicks::Now();
  last_upload_ = base::TimeTicks::Now();
  const std::string request_key = base::UnguessableToken::Create().ToString();

  // Setup downstream fetcher.
  GURL downstream_url(base::StringPrintf(
      "%s%s?key=%s&pair=%s&output=pb", kWebServiceBaseUrl, kDownstreamUrl,
      net::EscapeQueryParamValue(google_apis::GetAPIKey(), true).c_str(),
      net::EscapeQueryParamValue(request_key, true).c_str()));

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("cloud_speech_recognition",
                                          R"(
        semantics {
          sender: "Speech Recognition"
          description:
            "Chrome provides transcription from output audio by using the "
            "Google speech recognition web service. Audio is sent to Google's "
            "servers (upstream) and text is returned (downstream). This "
            "network request (downstream) sends an id for getting the text "
            "response. Then the (upstream) request sends the audio data along "
            "with the id. When the server has finished processing the audio "
            "data and produced a text response, it replies to this request."
          trigger:
            "Generally triggered in direct response to a user playing a "
            "media with audio."
          data: "A unique random id for this speech recognition request and "
            "the audio output stream."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "The Live Caption feature can be enabled/disabled in the Chrome "
            "accessibility settings menu. The feature is disabled by default."
          chrome_policy {
            AudioCaptureAllowed {
              policy_options {mode: MANDATORY}
              AudioCaptureAllowed: false
            }
          }
          chrome_policy {
            AudioCaptureAllowedUrls {
              policy_options {mode: MANDATORY}
              AudioCaptureAllowedUrls: {}
            }
          }
        })");
  auto downstream_request = std::make_unique<network::ResourceRequest>();
  downstream_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  downstream_request->url = downstream_url;
  downstream_loader_ = std::make_unique<speech::DownstreamLoader>(
      std::move(downstream_request), traffic_annotation,
      url_loader_factory_.get(), this);

  // Setup upstream fetcher.
  GURL upstream_url(base::StringPrintf(
      "%s%s?key=%s&pair=%s&output=pb&lang=%s&pFilter=0&maxAlternatives=1&app="
      "chrome&continuous&interim",
      kWebServiceBaseUrl, kUpstreamUrl,
      net::EscapeQueryParamValue(google_apis::GetAPIKey(), true).c_str(),
      net::EscapeQueryParamValue(request_key, true).c_str(),
      net::EscapeQueryParamValue(language_code_, true).c_str()));

  auto upstream_request = std::make_unique<network::ResourceRequest>();
  upstream_request->url = upstream_url;
  upstream_request->method = "POST";
  upstream_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  upstream_request->headers.SetHeader(
      net::HttpRequestHeaders::kContentType,
      "audio/l16; rate=" + base::NumberToString(sample_rate_));
  upstream_loader_ = std::make_unique<speech::UpstreamLoader>(
      std::move(upstream_request), traffic_annotation,
      url_loader_factory_.get(), this);
}

void CloudSpeechRecognitionClient::AddAudio(base::span<const char> chunk) {
  DCHECK(is_initialized_);
  base::TimeTicks now = base::TimeTicks::Now();
  if (now - last_reset_ > kMaximumStreamDuration ||
      now - last_upload_ > kMaximumPauseDuration) {
    Reset();
  }

  last_upload_ = now;
  upstream_loader_->AppendChunkToUpload(std::string(chunk.data(), chunk.size()),
                                        false);
}

void CloudSpeechRecognitionClient::SetUrlLoaderFactoryForTesting(
    mojo::PendingRemote<network::mojom::URLLoaderFactory> factory) {
  url_loader_factory_ =
      mojo::Remote<network::mojom::URLLoaderFactory>(std::move(factory));
}

void CloudSpeechRecognitionClient::ResetUrlLoaderFactory() {
  downstream_loader_.reset();
  upstream_loader_.reset();
  url_loader_factory_.reset();

  if (!speech_recognition_service_impl_)
    return;

  url_loader_factory_ = mojo::Remote<network::mojom::URLLoaderFactory>(
      speech_recognition_service_impl_->GetUrlLoaderFactory());

  url_loader_factory_.set_disconnect_handler(
      base::BindOnce(&CloudSpeechRecognitionClient::ResetUrlLoaderFactory,
                     base::Unretained(this)));

  if (!is_initialized_)
    return;

  Reset();
}

}  // namespace speech
