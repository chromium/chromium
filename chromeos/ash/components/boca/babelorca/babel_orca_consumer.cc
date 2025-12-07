// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/babel_orca_consumer.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_caption_translator.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_controller.h"
#include "chromeos/ash/components/boca/babelorca/caption_controller.h"
#include "chromeos/ash/components/boca/babelorca/oauth_token_fetcher.h"
#include "chromeos/ash/components/boca/babelorca/request_data_wrapper.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_authed_client.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_authed_client_impl.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_client_impl.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_request_data_provider.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_response.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_streaming_client.h"
#include "chromeos/ash/components/boca/babelorca/token_manager.h"
#include "chromeos/ash/components/boca/babelorca/token_manager_impl.h"
#include "chromeos/ash/components/boca/babelorca/transcript_receiver.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "chromeos/ash/services/boca/babelorca/mojom/tachyon_parsing_service.mojom.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/service_process_host.h"
#include "google_apis/gaia/gaia_id.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash::babelorca {
namespace {

constexpr char kReceivingStoppedReasonUma[] =
    "Ash.Boca.Babelorca.ReceivingStoppedReason";

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("ash_babelorca_babel_orca_consumer",
                                        R"(
        semantics {
          sender: "School Tools"
          description: "Request to joing group to receive School Tools session"
                        "captions."
          trigger: "User enables receiving speech captions during a School "
                    "Tools session."
          data: "OAuth token, School Tools session id and gaia id for request "
                "verification."
          user_data {
            type: ACCESS_TOKEN
            type: GAIA_ID
            type: OTHER
          }
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "cros-edu-eng@google.com"
            }
          }
          last_reviewed: "2024-10-18"
        }
        policy {
          cookies_allowed: NO
          setting: "This request cannot be stopped in settings, but will not "
                    "be sent if the user does not enable receiving captions in "
                    "School Tools session."
          policy_exception_justification: "Not implemented."
        })");

mojo::Remote<babelorca::mojom::TachyonParsingService> BindTachyonService() {
  return content::ServiceProcessHost::Launch<
      babelorca::mojom::TachyonParsingService>(
      content::ServiceProcessHost::Options()
          .WithDisplayName("BabelOrca Tachyon Parsing Service")
          .Pass());
}

std::unique_ptr<TachyonAuthedClient> CreateStreamingClient(
    TokenManager* token_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    TachyonStreamingClient::OnMessageCallback on_message_callback) {
  auto streaming_client = std::make_unique<TachyonStreamingClient>(
      url_loader_factory, base::BindRepeating(BindTachyonService),
      std::move(on_message_callback));
  return std::make_unique<TachyonAuthedClientImpl>(std::move(streaming_client),
                                                   token_manager);
}

}  // namespace

// static
std::unique_ptr<BabelOrcaController> BabelOrcaConsumer::Create(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    GaiaId gaia_id,
    std::string school_tools_base_url,
    std::unique_ptr<CaptionController> caption_controller,
    std::unique_ptr<BabelOrcaCaptionTranslator> translator,
    PrefService* pref_service,
    TokenManager* tachyon_oauth_token_manager,
    TachyonRequestDataProvider* tachyon_request_data_provider) {
  auto streaming_client_getter =
      base::BindRepeating(CreateStreamingClient, tachyon_oauth_token_manager);
  return std::make_unique<BabelOrcaConsumer>(
      url_loader_factory, identity_manager, gaia_id, school_tools_base_url,
      std::move(caption_controller), tachyon_oauth_token_manager,
      tachyon_request_data_provider, std::move(streaming_client_getter),
      std::move(translator), pref_service);
}

BabelOrcaConsumer::BabelOrcaConsumer(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    const GaiaId& gaia_id,
    std::string school_tools_base_url,
    std::unique_ptr<CaptionController> caption_controller,
    TokenManager* tachyon_oauth_token_manager,
    TachyonRequestDataProvider* tachyon_request_data_provider,
    TranscriptReceiver::StreamingClientGetter streaming_client_getter,
    std::unique_ptr<BabelOrcaCaptionTranslator> translator,
    PrefService* pref_service)
    : url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager),
      gaia_id_(gaia_id),
      school_tools_base_url_(std::move(school_tools_base_url)),
      caption_controller_(std::move(caption_controller)),
      tachyon_oauth_token_manager_(tachyon_oauth_token_manager),
      tachyon_request_data_provider_(tachyon_request_data_provider),
      streaming_client_getter_(std::move(streaming_client_getter)),
      pref_service_(pref_service),
      pref_change_registrar_(std::make_unique<PrefChangeRegistrar>()),
      translator_(std::move(translator)) {
  pref_change_registrar_->Init(pref_service_);
}

BabelOrcaConsumer::~BabelOrcaConsumer() {
  VLOG(1) << "[BabelOrca] stop receiving in dtor";
  StopReceiving();
}

void BabelOrcaConsumer::OnSessionStarted() {
  VLOG(1) << "[BabelOrca] session started";
  in_session_ = true;
}

void BabelOrcaConsumer::OnSessionEnded() {
  if (IsReceivingCaptions()) {
    base::UmaHistogramEnumeration(kReceivingStoppedReasonUma,
                                  ReceivingStoppedReason::kSessionEnded);
  }
  VLOG(1) << "[BabelOrca] session ended";
  in_session_ = false;
  Reset();
}

void BabelOrcaConsumer::OnSessionCaptionConfigUpdated(
    bool session_captions_enabled,
    bool translations_enabled) {
  if (IsReceivingCaptions() && !session_captions_enabled) {
    base::UmaHistogramEnumeration(
        kReceivingStoppedReasonUma,
        ReceivingStoppedReason::kSessionCaptionTurnedOff);
  }
  if (!in_session_) {
    LOG(ERROR)
        << "[BabelOrca] Session caption config event called out of session.";
    return;
  }
  session_captions_enabled_ = session_captions_enabled;
  if (features::IsBocaTranslateToggleEnabled()) {
    caption_controller_->SetTranslateAllowed(translations_enabled);
  } else {
    caption_controller_->SetLiveTranslateEnabled(translations_enabled);
  }
  if (!session_captions_enabled_) {
    VLOG(1) << "[BabelOrca] session caption disabled, stop receiving";
    StopReceiving();
    return;
  }
  signed_in_ = tachyon_request_data_provider_->tachyon_token().has_value();
  StartReceiving();
}

void BabelOrcaConsumer::OnLocalCaptionConfigUpdated(
    bool local_captions_enabled) {
  if (IsReceivingCaptions() && !local_captions_enabled) {
    base::UmaHistogramEnumeration(
        kReceivingStoppedReasonUma,
        ReceivingStoppedReason::kLocalCaptionTurnedOff);
  }
  if (!in_session_) {
    LOG(ERROR) << "Consumer local caption config event called out of session.";
    return;
  }
  local_captions_enabled_ = local_captions_enabled;
  if (!local_captions_enabled_) {
    VLOG(1) << "[BabelOrca] local caption disabled, stop receiving";
    StopReceiving();
    return;
  }
  StartReceiving();
}

bool BabelOrcaConsumer::IsProducer() {
  return false;
}

void BabelOrcaConsumer::DispatchTranscription(
    const media::SpeechRecognitionResult& result) {
  bool dispatch_success = caption_controller_->DispatchTranscription(result);
  // TODO(crbug.com/373692250): add dispatch attempts error limit and report
  // failure.
  VLOG_IF(1, !dispatch_success)
      << "Caption bubble transcription dispatch failed";
}

// TODO(377544551): Enforce ordering of results passed back to us.
void BabelOrcaConsumer::OnTranslationCallback(
    const std::optional<media::SpeechRecognitionResult>& result) {
  if (result) {
    DispatchTranscription(result.value());
  } else {
    VLOG(1) << "Translation dispatch failed";
  }
}


void BabelOrcaConsumer::StartReceiving() {
  if (!local_captions_enabled_ || !session_captions_enabled_) {
    VLOG(1) << "[BabelOrca] receiving will not start, local captions is "
            << local_captions_enabled_ << " and session captions is "
            << session_captions_enabled_;
    return;
  }
  if (!signed_in_) {
    VLOG(1) << "[BabelOrca] not signed in, signin to tachyon and respond";
    tachyon_request_data_provider_->SigninToTachyonAndRespond(base::BindOnce(
        &BabelOrcaConsumer::OnSignedIn, weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  if (!joined_group_) {
    VLOG(1) << "[BabelOrca] join group";
    JoinSessionTachyonGroup();
    return;
  }
  VLOG(1) << "[BabelOrca] create bubble and start receiving";
  caption_controller_->StartLiveCaption();
  transcript_receiver_ = std::make_unique<TranscriptReceiver>(
      url_loader_factory_, tachyon_request_data_provider_,
      streaming_client_getter_);
  transcript_receiver_->StartReceiving(
      base::BindRepeating(&BabelOrcaConsumer::OnTranscriptReceived,
                          base::Unretained(this)),
      base::BindOnce(&BabelOrcaConsumer::OnReceivingFailed,
                     base::Unretained(this)));
}

void BabelOrcaConsumer::OnSignedIn(bool success) {
  if (IsReceivingCaptions() && !success) {
    base::UmaHistogramEnumeration(kReceivingStoppedReasonUma,
                                  ReceivingStoppedReason::kTachyonSigninError);
  }
  if (!success) {
    // TODO(crbug.com/373692250): report error.
    LOG(ERROR) << "[BabelOrca] Failed to signin to Tachyon";
    return;
  }
  signed_in_ = true;
  StartReceiving();
}

void BabelOrcaConsumer::JoinSessionTachyonGroup() {
  if (!tachyon_request_data_provider_->session_id().has_value()) {
    // TODO(crbug.com/373692250): report error.
    LOG(ERROR) << "[BabelOrca] Session id is not set";
    return;
  }
  join_group_authed_client_.reset();
  auto oauth_token_fetcher = std::make_unique<OAuthTokenFetcher>(
      identity_manager_, signin::OAuthConsumerId::kChromeOsBocaSchoolToolsAuth,
      /*uma_name=*/"SchoolTools");
  join_group_token_manager_ =
      std::make_unique<TokenManagerImpl>(std::move(oauth_token_fetcher));
  join_group_authed_client_ = std::make_unique<TachyonAuthedClientImpl>(
      std::make_unique<TachyonClientImpl>(url_loader_factory_),
      join_group_token_manager_.get());
  join_group_url_ =
      base::StrCat({school_tools_base_url_,
                    base::ReplaceStringPlaceholders(
                        boca::kJoinTachyonGroupUrlTemplate,
                        {gaia_id_.ToString(),
                         tachyon_request_data_provider_->session_id().value()},
                        /*=offsets*/ nullptr)});
  auto request_data = std::make_unique<RequestDataWrapper>(
      kTrafficAnnotation, join_group_url_, /*max_retries_param=*/3,
      base::BindOnce(&BabelOrcaConsumer::OnJoinGroupResponse,
                     base::Unretained(this)),
      boca::kContentTypeApplicationJson);
  request_data->uma_name = "JoinGroup";
  join_group_authed_client_->StartAuthedRequestString(std::move(request_data),
                                                      "");
}

void BabelOrcaConsumer::OnJoinGroupResponse(TachyonResponse response) {
  if (IsReceivingCaptions() && !response.ok()) {
    base::UmaHistogramEnumeration(kReceivingStoppedReasonUma,
                                  ReceivingStoppedReason::kJoinGroupError);
  }
  if (!response.ok()) {
    // TODO(crbug.com/373692250): report error.
    LOG(ERROR) << "[BabelOrca] Failed to join Tachyon group";
    return;
  }
  VLOG(1) << "[BabelOrca] group joined, start receiving";
  joined_group_ = true;
  StartReceiving();
}

void BabelOrcaConsumer::OnTranscriptReceived(
    media::SpeechRecognitionResult transcript,
    std::string language) {
  if (!transcript_lang_.has_value() || transcript_lang_.value() != language) {
    transcript_lang_ = language;
    caption_controller_->OnLanguageIdentificationEvent(
        media::mojom::LanguageIdentificationEvent::New(
            /*language=*/language, media::mojom::ConfidenceLevel::kConfident,
            media::mojom::AsrSwitchResult::kSwitchSucceeded));
  }
  if (caption_controller_->IsTranslateAllowedAndEnabled()) {
    translator_->Translate(
        transcript,
        base::BindOnce(&BabelOrcaConsumer::DispatchTranscription,
                       weak_ptr_factory_.GetWeakPtr()),
        language, caption_controller_->GetLiveTranslateTargetLanguageCode());
    return;
  }

  DispatchTranscription(transcript);
}

void BabelOrcaConsumer::OnReceivingFailed() {
  if (IsReceivingCaptions()) {
    base::UmaHistogramEnumeration(
        kReceivingStoppedReasonUma,
        ReceivingStoppedReason::kTachyonReceiveMessagesError);
  }
  // TODO(crbug.com/373692250): report error.
  LOG(ERROR) << "[BabelOrca] Transcript receive request failed";
  // Only reset local captions since session caption is not controlled by the
  // consumer.
  local_captions_enabled_ = false;
  StopReceiving();
}

void BabelOrcaConsumer::StopReceiving() {
  caption_controller_->StopLiveCaption();
  transcript_receiver_.reset();
}

void BabelOrcaConsumer::Reset() {
  StopReceiving();
  local_captions_enabled_ = false;
  session_captions_enabled_ = false;
  joined_group_ = false;
}

bool BabelOrcaConsumer::IsReceivingCaptions() {
  return in_session_ && local_captions_enabled_ && session_captions_enabled_;
}

}  // namespace ash::babelorca
