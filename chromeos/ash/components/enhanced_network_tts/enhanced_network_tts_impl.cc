// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/enhanced_network_tts/enhanced_network_tts_impl.h"

#include <iterator>
#include <utility>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/enhanced_network_tts/enhanced_network_tts_constants.h"
#include "chromeos/ash/components/enhanced_network_tts/enhanced_network_tts_utils.h"
#include "components/google/core/common/google_util.h"
#include "google_apis/google_api_keys.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

namespace ash::enhanced_network_tts {

BASE_FEATURE(kEnhancedNetworkTtsOverride,
             "EnhancedNetworkTtsOverride",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<std::string> EnhancedNetworkTtsImpl::kApiKey;

const net::NetworkTrafficAnnotationTag traffic_annotation =
    net::DefineNetworkTrafficAnnotation("enhanced_network_tts", R"(
    semantics {
      sender: "Enhanced Network TTS"
      description:
        "This is the middle layer of a text-to-speech engine implemented using the "
        "chrome.ttsEngine extension api. "
        "It takes extension api requests from the api to speak a string, sends a "
        "network request to a network endpoint, and replies with audio samples and "
        "related metadata. "
        "It is only used by Select to Speak in Chrome OS."
      trigger: "Turn on Select-to-speak from settings. "
        "Use search +s or hold search and click some text to get it to start. "
        "Accept the dialog about natural voices, and/or go to Select-to-speak to enable them."
      user_data {
        type: WEB_CONTENT
      }
      data: "1. Google API Key."
            "2. Text piece to be converted to speech."
            "3. Voice to be used for the speech."
            "4. Language of the speech."
            "5. Rate of the speech."
      destination: GOOGLE_OWNED_SERVICE
      internal {
        contacts {
          email: "chrome-a11y-core@google.com"
        }
      }
      last_reviewed: "2023-04-05"
    }
    policy {
      cookies_allowed: NO
      setting:
        "Users can disable the text-to-speech with enhanced voices feature on "
        "the Select-to-Speak subpage of the Accessibility settings. This "
        "setting is disabled by default."
      policy_exception_justification: "Not implemented."
    })");

EnhancedNetworkTtsImpl::ServerRequest::ServerRequest(
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    int start_index,
    bool is_last_request)
    : url_loader(std::move(url_loader)),
      start_index(start_index),
      is_last_request(is_last_request) {}

EnhancedNetworkTtsImpl::ServerRequest::~ServerRequest() = default;

EnhancedNetworkTtsImpl& EnhancedNetworkTtsImpl::GetInstance() {
  static base::NoDestructor<EnhancedNetworkTtsImpl> tts_impl;
  return *tts_impl;
}

EnhancedNetworkTtsImpl::EnhancedNetworkTtsImpl()
    : api_key_(kApiKey.Get().empty() ? google_apis::GetReadAloudAPIKey()
                                     : kApiKey.Get()),
      char_limit_per_request_(mojom::kEnhancedNetworkTtsMaxCharacterSize) {}
EnhancedNetworkTtsImpl::~EnhancedNetworkTtsImpl() = default;

void EnhancedNetworkTtsImpl::BindReceiverAndURLFactory(
    mojo::PendingReceiver<mojom::EnhancedNetworkTts> receiver,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  // Reset the receiver in case of rebinding (e.g., after the extension crash).
  receiver_.reset();
  receiver_.Bind(std::move(receiver));

  url_loader_factory_ = url_loader_factory;
}

void EnhancedNetworkTtsImpl::GetAudioData(mojom::TtsRequestPtr request,
                                          GetAudioDataCallback callback) {
  // Reset if we have bound observer from the prior TtsRequest.
  if (on_data_received_observer_.is_bound()) {
    ResetAndSendErrorResponse(mojom::TtsRequestError::kRequestOverride);
    DVLOG(1) << "Multiple requests for Enhance Network TTS, override the "
                "prior one.";
  }

  auto pending_receiver =
      on_data_received_observer_.BindNewPipeAndPassReceiver();
  std::move(callback).Run(std::move(pending_receiver));
  // If the message pipe is disconnected, then the caller is no longer
  // interested in receiving the result of processing `request`, so reset the
  // internal state.
  on_data_received_observer_.set_disconnect_handler(
      base::BindOnce(&EnhancedNetworkTtsImpl::ResetServerRequestsAndObserver,
                     weak_factory_.GetWeakPtr()));

  // Return early if the utterance is empty.
  if (request->utterance.empty()) {
    ResetAndSendErrorResponse(mojom::TtsRequestError::kEmptyUtterance);
    return;
  }

  std::u16string utterance_u16string = base::UTF8ToUTF16(request->utterance);
  // Ignore the whitespaces at start. The ICU break iterator does not work well
  // with text that has whitespaces at start. We must trim the text before
  // sending it to |FindTextBreaks|.
  int start_offset = 0;
  while (base::IsUnicodeWhitespace(utterance_u16string[start_offset])) {
    start_offset++;
  }
  utterance_u16string = utterance_u16string.substr(start_offset);

  // Chop the utterance into smaller text pieces and queue them into
  // |server_requests_|.
  std::vector<uint16_t> text_breaks =
      FindTextBreaks(utterance_u16string, char_limit_per_request_);
  uint16_t text_piece_start_index = 0;
  for (size_t i = 0; i < text_breaks.size(); i++) {
    uint16_t text_piece_end_index = text_breaks[i];
    auto size = text_piece_end_index - text_piece_start_index + 1;
    const std::string text_piece = base::UTF16ToUTF8(
        utterance_u16string.substr(text_piece_start_index, size));

    mojom::TtsRequestPtr new_tts_request = mojom::TtsRequest::New(
        text_piece, request->rate, request->voice, request->lang);
    std::unique_ptr<network::SimpleURLLoader> url_loader = MakeRequestLoader();
    const bool last_request = i == text_breaks.size() - 1;
    url_loader->AttachStringForUpload(
        FormatJsonRequest(std::move(new_tts_request)),
        kNetworkRequestUploadType);
    server_requests_.emplace_back(std::move(url_loader),
                                  text_piece_start_index + start_offset,
                                  last_request);

    // Prepare for the next text piece.
    text_piece_start_index = text_piece_end_index + 1;
  }

  // Kick off the server requests.
  ProcessNextServerRequest();
}

void EnhancedNetworkTtsImpl::SetCharLimitPerRequestForTesting(int limit) {
  char_limit_per_request_ = limit;
}

std::unique_ptr<network::SimpleURLLoader>
EnhancedNetworkTtsImpl::MakeRequestLoader() {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "POST";
  const GURL server_url = GURL(kReadAloudServerUrl);
  resource_request->url = server_url;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  // Put API key in request's header if a key exists, and the endpoint is
  // trusted by Google.
  if (!api_key_.empty() && server_url.SchemeIs(url::kHttpsScheme) &&
      google_util::IsGoogleAssociatedDomainUrl(server_url)) {
    resource_request->headers.SetHeader(kGoogApiKeyHeader, api_key_);
  }

  return network::SimpleURLLoader::Create(std::move(resource_request),
                                          traffic_annotation);
}

void EnhancedNetworkTtsImpl::ProcessNextServerRequest() {
  // If there is no more request to process, resets the state variables and
  // return early.
  if (server_requests_.empty()) {
    ResetServerRequestsAndObserver();
    return;
  }

  const ServerRequestList::iterator first_request_it = server_requests_.begin();
  network::SimpleURLLoader::BodyAsStringCallbackDeprecated
      body_as_string_callback =
          base::BindOnce(&EnhancedNetworkTtsImpl::OnServerResponseReceived,
                         weak_factory_.GetWeakPtr(), first_request_it);
  server_requests_.front().url_loader->DownloadToString(
      url_loader_factory_.get(), std::move(body_as_string_callback),
      kEnhancedNetworkTtsMaxResponseSize);
}

void EnhancedNetworkTtsImpl::OnServerResponseReceived(
    const ServerRequestList::iterator server_request_it,
    const std::unique_ptr<std::string> json_response) {
  // This callback will not be called when the url_loader and its request are
  // deleted. See simple_url_loader.h for more details.
  DCHECK(!server_requests_.empty());
  // The iterator should only point to the begin of the list.
  DCHECK(server_requests_.begin() == server_request_it);

  const int start_index = server_request_it->start_index;
  const bool is_last_request = server_request_it->is_last_request;

  // Remove the current request from the list.
  server_requests_.erase(server_request_it);

  if (!json_response) {
    DVLOG(1) << "HTTP request for Enhance Network TTS failed.";
    ResetAndSendErrorResponse(mojom::TtsRequestError::kServerError);
    return;
  }

  // Send the JSON string to a dedicated service for safe parsing.
  data_decoder_.ParseJson(
      *json_response,
      base::BindOnce(&EnhancedNetworkTtsImpl::OnResponseJsonParsed,
                     weak_factory_.GetWeakPtr(), start_index, is_last_request));
}

void EnhancedNetworkTtsImpl::OnResponseJsonParsed(
    const int start_index,
    const bool is_last_request,
    data_decoder::DataDecoder::ValueOrError result) {
  // Extract results for the request.
  if (result.has_value() && result->is_list()) {
    SendResponse(
        UnpackJsonResponse(result->GetList(), start_index, is_last_request));
    // Only start the next request after finishing the current one. This method
    // will also reset the internal state if there is no more request.
    ProcessNextServerRequest();
  } else {
    ResetAndSendErrorResponse(mojom::TtsRequestError::kReceivedUnexpectedData);
    DVLOG(1) << "Parsing server response JSON failed with error: "
             << (!result.has_value() || result.error().empty()
                     ? "No reason reported."
                     : result.error());
  }
}

void EnhancedNetworkTtsImpl::SendResponse(mojom::TtsResponsePtr response) {
  if (on_data_received_observer_.is_bound()) {
    on_data_received_observer_->OnAudioDataReceived(std::move(response));
  }
}

void EnhancedNetworkTtsImpl::ResetServerRequestsAndObserver() {
  server_requests_.clear();
  on_data_received_observer_.reset();
}

void EnhancedNetworkTtsImpl::ResetAndSendErrorResponse(
    mojom::TtsRequestError error_code) {
  SendResponse(GetResultOnError(error_code));
  ResetServerRequestsAndObserver();
}

}  // namespace ash::enhanced_network_tts
