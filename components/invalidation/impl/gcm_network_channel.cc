// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/gcm_network_channel.h"

#include <utility>

#include "base/base64url.h"
#include "base/bind.h"
#include "base/hash/sha1.h"
#include "base/i18n/time_formatting.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/invalidation/impl/gcm_network_channel_delegate.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

#if !defined(OS_ANDROID)
// channel_common.proto defines ANDROID constant that conflicts with Android
// build. At the same time TiclInvalidationService is not used on Android so it
// is safe to exclude these protos from Android build.
#include "google/cacheinvalidation/android_channel.pb.h"
#include "google/cacheinvalidation/channel_common.pb.h"
#include "google/cacheinvalidation/types.pb.h"
#endif

namespace syncer {

namespace {

const char kCacheInvalidationEndpointUrl[] =
    "https://clients4.google.com/invalidation/android/request/";
const char kCacheInvalidationPackageName[] = "com.google.chrome.invalidations";

// Register backoff policy.
const net::BackoffEntry::Policy kRegisterBackoffPolicy = {
  // Number of initial errors (in sequence) to ignore before applying
  // exponential back-off rules.
  0,

  // Initial delay for exponential back-off in ms.
  2000, // 2 seconds.

  // Factor by which the waiting time will be multiplied.
  2,

  // Fuzzing percentage. ex: 10% will spread requests randomly
  // between 90%-100% of the calculated time.
  0.2, // 20%.

  // Maximum amount of time we are willing to delay our request in ms.
  1000 * 3600 * 4, // 4 hours.

  // Time to keep an entry from being discarded even when it
  // has no significant state, -1 to never discard.
  -1,

  // Don't use initial delay unless the last request was an error.
  false,
};

// Incoming message status values for UMA_HISTOGRAM.
enum IncomingMessageStatus {
  INCOMING_MESSAGE_SUCCESS,
  MESSAGE_EMPTY,     // GCM message's content is missing or empty.
  INVALID_ENCODING,  // Base64Decode failed.
  INVALID_PROTO,     // Parsing protobuf failed.

  // This enum is used in UMA_HISTOGRAM_ENUMERATION. Insert new values above
  // this line.
  INCOMING_MESSAGE_STATUS_COUNT
};

// Outgoing message status values for UMA_HISTOGRAM.
enum OutgoingMessageStatus {
  OUTGOING_MESSAGE_SUCCESS,
  MESSAGE_DISCARDED,     // New message started before old one was sent.
  ACCESS_TOKEN_FAILURE,  // Requeting access token failed.
  POST_FAILURE,          // HTTP Post failed.

  // This enum is used in UMA_HISTOGRAM_ENUMERATION. Insert new values above
  // this line.
  OUTGOING_MESSAGE_STATUS_COUNT
};

const char kIncomingMessageStatusHistogram[] =
    "GCMInvalidations.IncomingMessageStatus";
const char kOutgoingMessageStatusHistogram[] =
    "GCMInvalidations.OutgoingMessageStatus";

void RecordIncomingMessageStatus(IncomingMessageStatus status) {
  UMA_HISTOGRAM_ENUMERATION(kIncomingMessageStatusHistogram,
                            status,
                            INCOMING_MESSAGE_STATUS_COUNT);
}

void RecordOutgoingMessageStatus(OutgoingMessageStatus status) {
  UMA_HISTOGRAM_ENUMERATION(kOutgoingMessageStatusHistogram,
                            status,
                            OUTGOING_MESSAGE_STATUS_COUNT);
}

}  // namespace

GCMNetworkChannel::GCMNetworkChannel(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    network::NetworkConnectionTracker* network_connection_tracker,
    std::unique_ptr<GCMNetworkChannelDelegate> delegate)
    : url_loader_factory_(std::move(url_loader_factory)),
      network_connection_tracker_(network_connection_tracker),
      delegate_(std::move(delegate)),
      register_backoff_entry_(new net::BackoffEntry(&kRegisterBackoffPolicy)),
      gcm_channel_online_(false),
      http_channel_online_(false),
      diagnostic_info_(this) {
  network_connection_tracker_->AddNetworkConnectionObserver(this);
  delegate_->Initialize(
      base::Bind(&GCMNetworkChannel::OnConnectionStateChanged,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&GCMNetworkChannel::OnStoreReset, weak_factory_.GetWeakPtr()));
  Register();
}

GCMNetworkChannel::~GCMNetworkChannel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  network_connection_tracker_->RemoveNetworkConnectionObserver(this);
}

void GCMNetworkChannel::Register() {
  delegate_->Register(base::Bind(&GCMNetworkChannel::OnRegisterComplete,
                                 weak_factory_.GetWeakPtr()));
}

void GCMNetworkChannel::OnRegisterComplete(
    const std::string& registration_id,
    gcm::GCMClient::Result result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result == gcm::GCMClient::SUCCESS) {
    DCHECK(!registration_id.empty());
    DVLOG(2) << "Got registration_id";
    register_backoff_entry_->Reset();
    registration_id_ = registration_id;
    if (!cached_message_.empty())
      RequestAccessToken();
  } else {
    DVLOG(2) << "Register failed: " << result;
    // Retry in case of transient error.
    switch (result) {
      case gcm::GCMClient::NETWORK_ERROR:
      case gcm::GCMClient::SERVER_ERROR:
      case gcm::GCMClient::TTL_EXCEEDED:
      case gcm::GCMClient::UNKNOWN_ERROR: {
        register_backoff_entry_->InformOfRequest(false);
        base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&GCMNetworkChannel::Register,
                           weak_factory_.GetWeakPtr()),
            register_backoff_entry_->GetTimeUntilRelease());
        break;
      }
      default:
        break;
    }
  }
  diagnostic_info_.registration_id_ = registration_id_;
  diagnostic_info_.registration_result_ = result;
}

void GCMNetworkChannel::SendMessage(const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!message.empty());
  DVLOG(2) << "SendMessage";
  diagnostic_info_.sent_messages_count_++;
  if (!cached_message_.empty()) {
    RecordOutgoingMessageStatus(MESSAGE_DISCARDED);
  }
  cached_message_ = message;

  if (!registration_id_.empty()) {
    RequestAccessToken();
  }
}

void GCMNetworkChannel::RequestAccessToken() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->RequestToken(base::Bind(&GCMNetworkChannel::OnGetTokenComplete,
                                     weak_factory_.GetWeakPtr()));
}

void GCMNetworkChannel::OnGetTokenComplete(
    const GoogleServiceAuthError& error,
    const std::string& token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cached_message_.empty() || registration_id_.empty()) {
    // Nothing to do.
    return;
  }

  if (error.state() != GoogleServiceAuthError::NONE) {
    // Requesting access token failed. Persistent errors will be reported by
    // token service. Just drop this request, cacheinvalidations will retry
    // sending message and at that time we'll retry requesting access token.
    DVLOG(1) << "RequestAccessToken failed: " << error.ToString();
    RecordOutgoingMessageStatus(ACCESS_TOKEN_FAILURE);
    // Message won't get sent. Notify that http channel doesn't work.
    UpdateHttpChannelState(false);
    cached_message_.clear();
    return;
  }
  DCHECK(!token.empty());
  // Save access token in case POST fails and we need to invalidate it.
  access_token_ = token;

  DVLOG(2) << "Got access token, sending message";
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("invalidation_service", R"(
        semantics {
          sender: "Invalidation service"
          description:
            "Chromium uses cacheinvalidation library to receive push "
            "notifications from the server about sync items (bookmarks, "
            "passwords, preferences, etc.) modified on other clients. It uses "
            "GCMClient to receive incoming messages. This request is used for "
            "client-to-server communications."
          trigger:
            "The first message is sent to register client with the server on "
            "Chromium startup. It is then sent periodically to confirm that "
            "the client is still online. After receiving notification about "
            "server changes, the client sends this request to acknowledge that "
            "the notification is processed."
          data:
            "Different in each use case:\n"
            "- Initial registration: Doesn't contain user data.\n"
            "- Updating the set of subscriptions: Contains server generated "
            "client_token and ObjectIds identifying subscriptions. ObjectId "
            "is not unique to user.\n"
            "- Invalidation acknowledgement: Contains client_token, ObjectId "
            "and server version for corresponding subscription. Version is not "
            "related to sync data, it is an internal concept of invalidations "
            "protocol."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled."
          policy_exception_justification:
            "Not implemented. Disabling InvalidationService might break "
            "features that depend on it. It makes sense to control top level "
            "features that use InvalidationService."
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = BuildUrl(registration_id_);
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = "POST";
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                      "Bearer " + access_token_);
  if (!echo_token_.empty()) {
    resource_request->headers.SetHeader("echo-token", echo_token_);
  }
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  simple_url_loader_->AttachStringForUpload(cached_message_,
                                            "application/x-protobuffer");
  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&GCMNetworkChannel::OnSimpleLoaderComplete,
                     base::Unretained(this)));
  // Clear message to prevent accidentally resending it in the future.
  cached_message_.clear();
}

void GCMNetworkChannel::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int net_error = simple_url_loader_->NetError();
  bool is_success = (net_error == net::OK);
  int response_code = -1;
  if (simple_url_loader_->ResponseInfo() &&
      simple_url_loader_->ResponseInfo()->headers) {
    response_code =
        simple_url_loader_->ResponseInfo()->headers->response_code();
  }
  simple_url_loader_.reset();
  diagnostic_info_.last_post_response_code_ =
      (response_code / 100 != 2 || is_success) ? response_code : net_error;

  if (response_code == net::HTTP_UNAUTHORIZED) {
    DVLOG(1) << "SimpleURLLoader failure: HTTP_UNAUTHORIZED";
    delegate_->InvalidateToken(access_token_);
  }

  if (!response_body) {
    DVLOG(1) << "SimpleURLLoader failure";
    RecordOutgoingMessageStatus(POST_FAILURE);
    // POST failed. Notify that http channel doesn't work.
    UpdateHttpChannelState(false);
    return;
  }

  RecordOutgoingMessageStatus(OUTGOING_MESSAGE_SUCCESS);
  // Successfully sent message. Http channel works.
  UpdateHttpChannelState(true);
  DVLOG(2) << "SimpleURLLoader success";
}

void GCMNetworkChannel::OnIncomingMessage(const std::string& message,
                                          const std::string& echo_token) {
#if !defined(OS_ANDROID)
  if (!echo_token.empty())
    echo_token_ = echo_token;
  diagnostic_info_.last_message_empty_echo_token_ = echo_token.empty();
  diagnostic_info_.last_message_received_time_ = base::Time::Now();

  if (message.empty()) {
    RecordIncomingMessageStatus(MESSAGE_EMPTY);
    return;
  }
  std::string data;
  if (!base::Base64UrlDecode(
          message, base::Base64UrlDecodePolicy::IGNORE_PADDING, &data)) {
    RecordIncomingMessageStatus(INVALID_ENCODING);
    return;
  }
  ipc::invalidation::AddressedAndroidMessage android_message;
  if (!android_message.ParseFromString(data) ||
      !android_message.has_message()) {
    RecordIncomingMessageStatus(INVALID_PROTO);
    return;
  }
  DVLOG(2) << "Deliver incoming message";
  RecordIncomingMessageStatus(INCOMING_MESSAGE_SUCCESS);
  UpdateGcmChannelState(true);
  DeliverIncomingMessage(android_message.message());
#else
  // This code shouldn't be invoked on Android.
  NOTREACHED();
#endif
}

void GCMNetworkChannel::OnConnectionStateChanged(bool online) {
  UpdateGcmChannelState(online);
}

void GCMNetworkChannel::OnStoreReset() {
  // TODO(crbug.com/661660): Tell server the registration ID is no longer valid.
  registration_id_.clear();
}

void GCMNetworkChannel::OnConnectionChanged(
    network::mojom::ConnectionType connection_type) {
  // Network connection is restored. Let's notify cacheinvalidations so it has
  // chance to retry.
  NotifyNetworkStatusChange(connection_type !=
                            network::mojom::ConnectionType::CONNECTION_NONE);
}

void GCMNetworkChannel::UpdateGcmChannelState(bool online) {
  if (gcm_channel_online_ == online)
    return;
  gcm_channel_online_ = online;
  InvalidatorState channel_state = TRANSIENT_INVALIDATION_ERROR;
  if (gcm_channel_online_ && http_channel_online_)
    channel_state = INVALIDATIONS_ENABLED;
  NotifyChannelStateChange(channel_state);
}

void GCMNetworkChannel::UpdateHttpChannelState(bool online) {
  if (http_channel_online_ == online)
    return;
  http_channel_online_ = online;
  InvalidatorState channel_state = TRANSIENT_INVALIDATION_ERROR;
  if (gcm_channel_online_ && http_channel_online_)
    channel_state = INVALIDATIONS_ENABLED;
  NotifyChannelStateChange(channel_state);
}

GURL GCMNetworkChannel::BuildUrl(const std::string& registration_id) {
  DCHECK(!registration_id.empty());

#if !defined(OS_ANDROID)
  ipc::invalidation::EndpointId endpoint_id;
  endpoint_id.set_c2dm_registration_id(registration_id);
  endpoint_id.set_client_key(std::string());
  endpoint_id.set_package_name(kCacheInvalidationPackageName);
  endpoint_id.mutable_channel_version()->set_major_version(
      ipc::invalidation::INITIAL);
  std::string endpoint_id_buffer;
  endpoint_id.SerializeToString(&endpoint_id_buffer);

  ipc::invalidation::NetworkEndpointId network_endpoint_id;
  network_endpoint_id.set_network_address(
      ipc::invalidation::NetworkEndpointId_NetworkAddress_ANDROID);
  network_endpoint_id.set_client_address(endpoint_id_buffer);
  std::string network_endpoint_id_buffer;
  network_endpoint_id.SerializeToString(&network_endpoint_id_buffer);

  std::string base64URLPiece;
  base::Base64UrlEncode(
      network_endpoint_id_buffer, base::Base64UrlEncodePolicy::OMIT_PADDING,
      &base64URLPiece);

  std::string url(kCacheInvalidationEndpointUrl);
  url += base64URLPiece;
  return GURL(url);
#else
  // This code shouldn't be invoked on Android.
  NOTREACHED();
  return GURL();
#endif
}

void GCMNetworkChannel::SetMessageReceiver(
    invalidation::MessageCallback* incoming_receiver) {
  delegate_->SetMessageReceiver(base::Bind(
      &GCMNetworkChannel::OnIncomingMessage, weak_factory_.GetWeakPtr()));
  SyncNetworkChannel::SetMessageReceiver(incoming_receiver);
}

void GCMNetworkChannel::RequestDetailedStatus(
    base::Callback<void(const base::DictionaryValue&)> callback) {
  callback.Run(*diagnostic_info_.CollectDebugData());
}

void GCMNetworkChannel::UpdateCredentials(const CoreAccountId& account_id,
                                          const std::string& token) {
  // Do nothing. We get access token by requesting it for every message.
}

int GCMNetworkChannel::GetInvalidationClientType() {
#if defined(OS_IOS)
  return ipc::invalidation::ClientType::CHROME_SYNC_GCM_IOS;
#else
  return ipc::invalidation::ClientType::CHROME_SYNC_GCM_DESKTOP;
#endif
}

void GCMNetworkChannel::ResetRegisterBackoffEntryForTest(
    const net::BackoffEntry::Policy* policy) {
  register_backoff_entry_.reset(new net::BackoffEntry(policy));
}

GCMNetworkChannelDiagnostic::GCMNetworkChannelDiagnostic(
    GCMNetworkChannel* parent)
    : parent_(parent),
      last_message_empty_echo_token_(false),
      last_post_response_code_(0),
      registration_result_(gcm::GCMClient::UNKNOWN_ERROR),
      sent_messages_count_(0) {}

std::unique_ptr<base::DictionaryValue>
GCMNetworkChannelDiagnostic::CollectDebugData() const {
  std::unique_ptr<base::DictionaryValue> status(new base::DictionaryValue);
  status->SetString("GCMNetworkChannel.Channel", "GCM");
  std::string reg_id_hash = base::SHA1HashString(registration_id_);
  status->SetString("GCMNetworkChannel.HashedRegistrationID",
                    base::HexEncode(reg_id_hash.c_str(), reg_id_hash.size()));
  status->SetString("GCMNetworkChannel.RegistrationResult",
                    GCMClientResultToString(registration_result_));
  status->SetBoolean("GCMNetworkChannel.HadLastMessageEmptyEchoToken",
                     last_message_empty_echo_token_);
  status->SetString(
      "GCMNetworkChannel.LastMessageReceivedTime",
      base::TimeFormatShortDateAndTime(last_message_received_time_));
  status->SetInteger("GCMNetworkChannel.LastPostResponseCode",
                     last_post_response_code_);
  status->SetInteger("GCMNetworkChannel.SentMessages", sent_messages_count_);
  status->SetInteger("GCMNetworkChannel.ReceivedMessages",
                     parent_->GetReceivedMessagesCount());
  return status;
}

std::string GCMNetworkChannelDiagnostic::GCMClientResultToString(
    const gcm::GCMClient::Result result) const {
#define ENUM_CASE(x) case x: return #x; break;
  switch (result) {
    ENUM_CASE(gcm::GCMClient::SUCCESS);
    ENUM_CASE(gcm::GCMClient::NETWORK_ERROR);
    ENUM_CASE(gcm::GCMClient::SERVER_ERROR);
    ENUM_CASE(gcm::GCMClient::TTL_EXCEEDED);
    ENUM_CASE(gcm::GCMClient::UNKNOWN_ERROR);
    ENUM_CASE(gcm::GCMClient::INVALID_PARAMETER);
    ENUM_CASE(gcm::GCMClient::ASYNC_OPERATION_PENDING);
    ENUM_CASE(gcm::GCMClient::GCM_DISABLED);
  }
  NOTREACHED();
  return "";
}

}  // namespace syncer
