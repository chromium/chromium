// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/cdm/starboard_decryptor_cast.h"

#include <cstdint>
#include <cstring>
#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/notimplemented.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chromecast/media/base/decrypt_context_impl.h"
#include "chromecast/starboard/chromecast/starboard_adapter/public/cast_starboard_api_adapter.h"
#include "chromecast/starboard/media/cdm/starboard_drm_key_tracker.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#include "google_apis/google_api_keys.h"
#include "media/base/cdm_callback_promise.h"
#include "media/base/eme_constants.h"

namespace chromecast {
namespace media {

namespace {

// The API key must be added.
constexpr char kProvisionServerUrlMinusKey[] =
    "https://www.googleapis.com/certificateprovisioning/v1/devicecertificates/"
    "create?key=";

// A decrypt context that fakes decryption, so that the real decryption can be
// done in Starboard.
class DummyDecryptContext : public DecryptContextImpl {
 public:
  DummyDecryptContext()
      : DecryptContextImpl(CastKeySystem::KEY_SYSTEM_WIDEVINE) {}

  ~DummyDecryptContext() override = default;

  // DecryptContextImpl implementation:
  void DecryptAsync(CastDecoderBuffer* buffer,
                    uint8_t* output_or_handle,
                    size_t data_offset,
                    bool clear_output,
                    DecryptCB decrypt_cb) override {
    LOG(FATAL) << "Decryption in cast is not allowed for the starboard "
                  "pipeline (decryption must be done in starboard itself, via "
                  "SbPlayer and SbDrmSystem).";
  }

  DecryptContextImpl::OutputType GetOutputType() const override {
    // This should force decryption to be done in the MediaPipelineBackend (see
    // CdmDecryptor::Decrypt for the relevant logic that reads this value).
    return OutputType::kSecure;
  }
};

std::string DrmKeyStatusToString(StarboardDrmKeyStatus status) {
  switch (status) {
    case kStarboardDrmKeyStatusUsable:
      return "kStarboardDrmKeyStatusUsable";
    case kStarboardDrmKeyStatusExpired:
      return "kStarboardDrmKeyStatusExpired";
    case kStarboardDrmKeyStatusReleased:
      return "kStarboardDrmKeyStatusReleased";
    case kStarboardDrmKeyStatusRestricted:
      return "kStarboardDrmKeyStatusRestricted";
    case kStarboardDrmKeyStatusDownscaled:
      return "kStarboardDrmKeyStatusDownscaled";
    case kStarboardDrmKeyStatusPending:
      return "kStarboardDrmKeyStatusPending";
    case kStarboardDrmKeyStatusError:
      return "kStarboardDrmKeyStatusError";
  }
}

// Converts a starboard DRM status to a CdmPromise exception. This must not be
// called for a success status. Defaults to NotSupportedError.
::media::CdmPromise::Exception StarboardDrmErrorStatusToCdmException(
    StarboardDrmStatus status) {
  switch (status) {
    case kStarboardDrmStatusSuccess:
      LOG(FATAL) << "Success status cannot be converted to a CDM Exception";
    case kStarboardDrmStatusTypeError:
      return ::media::CdmPromise::Exception::TYPE_ERROR;
    case kStarboardDrmStatusNotSupportedError:
      return ::media::CdmPromise::Exception::NOT_SUPPORTED_ERROR;
    case kStarboardDrmStatusInvalidStateError:
      return ::media::CdmPromise::Exception::INVALID_STATE_ERROR;
    case kStarboardDrmStatusQuotaExceededError:
      return ::media::CdmPromise::Exception::QUOTA_EXCEEDED_ERROR;
    case kStarboardDrmStatusUnknownError:
    default:
      return ::media::CdmPromise::Exception::NOT_SUPPORTED_ERROR;
  }
}

::media::CdmKeyInformation::KeyStatus ToMediaKeyStatus(
    StarboardDrmKeyStatus status) {
  switch (status) {
    case kStarboardDrmKeyStatusUsable:
      return ::media::CdmKeyInformation::KeyStatus::USABLE;
    case kStarboardDrmKeyStatusExpired:
      return ::media::CdmKeyInformation::KeyStatus::EXPIRED;
    case kStarboardDrmKeyStatusReleased:
      return ::media::CdmKeyInformation::KeyStatus::RELEASED;
    case kStarboardDrmKeyStatusRestricted:
      return ::media::CdmKeyInformation::KeyStatus::OUTPUT_RESTRICTED;
    case kStarboardDrmKeyStatusDownscaled:
      return ::media::CdmKeyInformation::KeyStatus::OUTPUT_DOWNSCALED;
    case kStarboardDrmKeyStatusPending:
      return ::media::CdmKeyInformation::KeyStatus::KEY_STATUS_PENDING;
    case kStarboardDrmKeyStatusError:
      return ::media::CdmKeyInformation::KeyStatus::INTERNAL_ERROR;
  }
}

::media::CdmMessageType ToCdmMessageType(StarboardDrmSessionRequestType type) {
  switch (type) {
    case kStarboardDrmSessionRequestTypeLicenseRequest:
      return ::media::CdmMessageType::LICENSE_REQUEST;
    case kStarboardDrmSessionRequestTypeLicenseRenewal:
      return ::media::CdmMessageType::LICENSE_RENEWAL;
    case kStarboardDrmSessionRequestTypeLicenseRelease:
      return ::media::CdmMessageType::LICENSE_RELEASE;
    case kStarboardDrmSessionRequestTypeIndividualizationRequest:
      return ::media::CdmMessageType::INDIVIDUALIZATION_REQUEST;
  }
}

}  // namespace

StarboardDecryptorCast::StarboardDecryptorCast(
    ::media::CreateFetcherCB create_provision_fetcher_cb,
    MediaResourceTracker* media_resource_tracker)
    : CastCdm(media_resource_tracker),
      create_provision_fetcher_cb_(std::move(create_provision_fetcher_cb)) {
  CHECK(base::SequencedTaskRunner::HasCurrentDefault());
  task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  LOG(INFO) << "StarboardDecryptorCast constructor, this=" << this;
}

void StarboardDecryptorCast::CreateSessionAndGenerateRequest(
    ::media::CdmSessionType session_type,
    ::media::EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data,
    std::unique_ptr<::media::NewSessionCdmPromise> promise) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  LOG(INFO) << "StarboardDecryptorCast::CreateSessionAndGenerateRequest";

  // Persistent session types are not supported, since SbDrmSystem does not
  // support load/remove.
  if (session_type == ::media::CdmSessionType::kPersistentLicense) {
    promise->reject(::media::CdmPromise::Exception::NOT_SUPPORTED_ERROR, 0,
                    "Persistent licenses are not supported by starboard.");
    return;
  }

  CHECK_EQ(session_type, ::media::CdmSessionType::kTemporary)
      << "Unsupported session type: " << static_cast<int>(session_type);

  queued_session_requests_.push(
      {base::BindOnce(
           &StarboardDecryptorCast::HandleCreateSessionAndGenerateRequest,
           weak_factory_.GetWeakPtr(), session_type, init_data_type, init_data),
       std::move(promise)});

  ProcessQueuedSessionRequests();
}

void StarboardDecryptorCast::HandleCreateSessionAndGenerateRequest(
    ::media::CdmSessionType session_type,
    ::media::EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data,
    std::unique_ptr<::media::NewSessionCdmPromise> promise) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!pending_session_setup_);

  pending_session_setup_ = true;
  std::string init_type;
  switch (init_data_type) {
    case ::media::EmeInitDataType::WEBM:
      init_type = "webm";
      break;
    case ::media::EmeInitDataType::CENC:
      init_type = "cenc";
      break;
    case ::media::EmeInitDataType::KEYIDS:
      init_type = "keyids";
      break;
    default:
      // Covers the case of the UNKOWN type, or any unrecognized types.
      promise->reject(
          ::media::CdmPromise::Exception::NOT_SUPPORTED_ERROR, 0,
          "Unsupported init_data_type: " +
              base::NumberToString(static_cast<int>(init_data_type)));
      return;
  }

  const int ticket = current_ticket_++;
  ticket_to_new_session_promise_[ticket] = std::move(promise);
  StarboardDrmWrapper::GetInstance().GenerateSessionUpdateRequest(
      this, ticket, init_type, init_data);
}

void StarboardDecryptorCast::LoadSession(
    ::media::CdmSessionType session_type,
    const std::string& session_id,
    std::unique_ptr<::media::NewSessionCdmPromise> promise) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  LOG(ERROR) << "StarboardDecryptorCast::LoadSession (not supported)";
  promise->reject(::media::CdmPromise::Exception::NOT_SUPPORTED_ERROR, 0,
                  "LoadSession is not supported by starboard.");
}

void StarboardDecryptorCast::UpdateSession(
    const std::string& web_session_id,
    const std::vector<uint8_t>& response,
    std::unique_ptr<::media::SimpleCdmPromise> promise) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  LOG(INFO) << "StarboardDecryptorCast::UpdateSession, web session id = "
            << web_session_id;

  const int ticket = current_ticket_++;
  ticket_to_simple_cdm_promise_[ticket] = std::move(promise);
  // This will eventually call OnSessionUpdated.
  StarboardDrmWrapper::GetInstance().UpdateSession(this, ticket, web_session_id,
                                                   response);
}

void StarboardDecryptorCast::CloseSession(
    const std::string& web_session_id,
    std::unique_ptr<::media::SimpleCdmPromise> promise) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  LOG(INFO) << "StarboardDecryptorCast::CloseSession, web session id = "
            << web_session_id;

  if (!session_ids_.contains(web_session_id)) {
    LOG(INFO) << "StarboardDecryptorCast::CloseSession did not find session ID "
              << web_session_id
              << ". It is possible this session was already closed; resolving "
                 "promise.";
    promise->resolve();
    return;
  }

  std::vector<std::unique_ptr<::media::SimpleCdmPromise>>& promises =
      session_id_to_simple_cdm_promises_[web_session_id];
  promises.push_back(std::move(promise));

  if (promises.size() == 1) {
    // This is the first request to close the session; mark the session as
    // removed and call starboard to perform the close logic
    StarboardDrmKeyTracker::GetInstance().RemoveKeysForSession(web_session_id);
    StarboardDrmWrapper::GetInstance().CloseSession(this, web_session_id);
  } else {
    LOG(INFO) << "Session " << web_session_id << " is currently closing.";
  }
}

void StarboardDecryptorCast::RemoveSession(
    const std::string& session_id,
    std::unique_ptr<::media::SimpleCdmPromise> promise) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  LOG(INFO)
      << "StarboardDecryptorCast::RemoveSession (implemented as CloseSession), "
         "session id = "
      << session_id;
  CloseSession(session_id, std::move(promise));
}

void StarboardDecryptorCast::SetServerCertificate(
    const std::vector<uint8_t>& certificate_data,
    std::unique_ptr<::media::SimpleCdmPromise> promise) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  LOG(INFO) << "StarboardDecryptorCast::SetServerCertificate";

  if (!server_certificate_updatable_) {
    LOG(ERROR) << "Tried to update a server certificate for a DRM system that "
                  "does not support it.";
    promise->reject(::media::CdmPromise::Exception::NOT_SUPPORTED_ERROR, 0,
                    "DRM system does not support certificate update");
    return;
  }

  const int ticket = current_ticket_++;
  ticket_to_simple_cdm_promise_[ticket] = std::move(promise);
  StarboardDrmWrapper::GetInstance().UpdateServerCertificate(this, ticket,
                                                             certificate_data);
}

std::unique_ptr<DecryptContextImpl> StarboardDecryptorCast::GetDecryptContext(
    const std::string& key_id,
    EncryptionScheme encryption_scheme) const {
  return std::make_unique<DummyDecryptContext>();
}

void StarboardDecryptorCast::SetKeyStatus(const std::string& key_id,
                                          CastKeyStatus key_status,
                                          uint32_t system_code) {
  NOTIMPLEMENTED();
}

void StarboardDecryptorCast::SetVideoResolution(int width, int height) {
  // This would normally be used to support per resolution keys, but is not
  // supported via any starboard API.
  LOG(ERROR) << "Called StarboardDecryptorCast::SetVideoResolution(" << width
             << ", " << height << "). This is not supported by starboard.";
}

StarboardDecryptorCast::~StarboardDecryptorCast() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  LOG(INFO) << "StarboardDecryptorCast destructor, this=" << this;
  for (const std::string& session_id : session_ids_) {
    StarboardDrmKeyTracker::GetInstance().RemoveKeysForSession(session_id);
  }
  RejectPendingPromises();
}

void StarboardDecryptorCast::InitializeInternal() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  server_certificate_updatable_ =
      StarboardDrmWrapper::GetInstance().IsServerCertificateUpdatable();
}

void StarboardDecryptorCast::RejectPendingPromises() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  while (!queued_session_requests_.empty()) {
    SessionRequest session_request =
        std::move(queued_session_requests_.front());
    queued_session_requests_.pop();
    if (session_request.second) {
      session_request.second->reject(
          ::media::CdmPromise::Exception::INVALID_STATE_ERROR, 0,
          "GenerateKeyRequest not completed by teardown");
    }
  }
}

void StarboardDecryptorCast::SendProvisionRequest(int ticket,
                                                  std::string session_id,
                                                  const std::string& content) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!provision_fetcher_) {
    provision_fetcher_ = create_provision_fetcher_cb_.Run();
  }
  LOG(INFO) << "Sending provision request";

  const std::string api_key = google_apis::GetAPIKey();
  if (api_key.empty()) {
    LOG(ERROR)
        << "Could not retrieve Google API key. Unable to provision the device.";
    return;
  }
  provision_fetcher_->Retrieve(
      GURL(base::StrCat({kProvisionServerUrlMinusKey, api_key})), content,
      base::BindOnce(&StarboardDecryptorCast::OnProvisionResponse,
                     weak_factory_.GetWeakPtr(), ticket,
                     std::move(session_id)));
}

void StarboardDecryptorCast::ProcessQueuedSessionRequests() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (queued_session_requests_.empty()) {
    return;
  }

  if (pending_session_setup_) {
    LOG(INFO) << "Queueing session request because another one is in flight";
    return;
  }

  SessionRequest session_request = std::move(queued_session_requests_.front());
  queued_session_requests_.pop();
  // This runs HandleCreateSessionAndGenerateRequest().
  std::move(session_request.first).Run(std::move(session_request.second));
}

void StarboardDecryptorCast::OnSessionUpdateRequest(
    int ticket,
    StarboardDrmStatus status,
    StarboardDrmSessionRequestType type,
    std::string error_message,
    std::string session_id,
    std::vector<uint8_t> content) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  pending_session_setup_ = false;

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&StarboardDecryptorCast::ProcessQueuedSessionRequests,
                     weak_factory_.GetWeakPtr()));

  if (status == kStarboardDrmStatusSuccess && !session_id.empty() &&
      type == kStarboardDrmSessionRequestTypeIndividualizationRequest) {
    // Provision requests need to be sent regardless of whether the ticket is
    // valid, so we send this before checking the ticket map.
    SendProvisionRequest(
        ticket, session_id,
        std::string(reinterpret_cast<const char*>(content.data()),
                    content.size()));
    return;
  }

  auto it = ticket_to_new_session_promise_.find(ticket);

  if (it == ticket_to_new_session_promise_.end()) {
    LOG(WARNING) << "Bad ticket for DRM session create request: " << ticket;
    if (!session_id.empty()) {
      CHECK_NE(type, kStarboardDrmSessionRequestTypeIndividualizationRequest);
      OnSessionMessage(session_id, content, ToCdmMessageType(type));
    }
    return;
  }

  // Regardless of success/failure, the promise at it->second must be
  // resolved/rejected and then deleted.
  if (status != kStarboardDrmStatusSuccess) {
    LOG(ERROR) << "Call to StarboardDrmGenerateSessionUpdateRequest for ticket "
               << it->first << "  failed with error: " << error_message;
    it->second->reject(StarboardDrmErrorStatusToCdmException(status), 0,
                       error_message);
  } else if (session_id.empty()) {
    LOG(ERROR)
        << "Starboard returned an empty session_id on success for ticket "
        << it->first;
    it->second->reject(::media::CdmPromise::Exception::INVALID_STATE_ERROR, 0,
                       "");
  } else {
    // Success case.
    LOG(INFO) << "Created session id " << session_id << " for ticket "
              << it->first;
    session_ids_.insert(session_id);
    it->second->resolve(session_id);
  }
  ticket_to_new_session_promise_.erase(it);
  if (status == kStarboardDrmStatusSuccess && !session_id.empty()) {
    base::span<const uint8_t> content_span = base::as_byte_span(content);
    // This function -- defined by the parent class -- should ultimately send
    // the message to the JS app. That will eventually trigger a license
    // request, followed by an update session message being sent to this class.
    // Note that, per the documentation of
    // ContentDecryptionModule::CreateSessionAndGenerateRequest, this must be
    // called AFTER the promise has been resolved.
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&StarboardDecryptorCast::OnSessionMessage,
                                  weak_factory_.GetWeakPtr(), session_id,
                                  std::vector<uint8_t>(content_span.begin(),
                                                       content_span.end()),
                                  ToCdmMessageType(type)));
  }
}

void StarboardDecryptorCast::OnProvisionResponse(int ticket,
                                                 const std::string& session_id,
                                                 bool success,
                                                 const std::string& response) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!success) {
    LOG(ERROR) << "Provisioning failed.";
    auto it = ticket_to_new_session_promise_.find(ticket);
    if (it == ticket_to_new_session_promise_.end()) {
      return;
    }
    it->second->reject(::media::CdmPromise::Exception::INVALID_STATE_ERROR,
                       /*system_code=*/0, "Widevine provisioning failed");
    return;
  }

  LOG(INFO) << "Provisioning succeeded. Updating session in starboard.";
  std::vector<uint8_t> response_vec(response.size());
  base::span<uint8_t>(response_vec)
      .copy_from_nonoverlapping(base::as_byte_span(response));

  // This will be called if we successfully update the session.
  auto success_cb =
      [](base::flat_map<int, std::unique_ptr<::media::NewSessionCdmPromise>>*
             promise_map,
         int ticket, std::string session_id) {
        CHECK(promise_map);
        LOG(INFO) << "UpdateSession for provision response succeeded";
        auto it = promise_map->find(ticket);
        if (it == promise_map->end()) {
          return;
        }
        it->second->resolve(session_id);
        promise_map->erase(it);
      };

  // This will be called if the session cannot be updated. The first two args
  // are bound here.
  auto failure_cb =
      [](base::flat_map<int, std::unique_ptr<::media::NewSessionCdmPromise>>*
             promise_map,
         int ticket, ::media::CdmPromise::Exception exception_code,
         uint32_t system_code, const std::string& error_message) {
        CHECK(promise_map);
        LOG(ERROR) << "UpdateSession for provision response failed with "
                      "exception "
                   << static_cast<int>(exception_code) << ", system code "
                   << system_code << ", and error message " << error_message;
        auto it = promise_map->find(ticket);
        if (it == promise_map->end()) {
          return;
        }
        it->second->reject(exception_code, system_code, error_message);
        promise_map->erase(it);
      };

  UpdateSession(
      session_id, response_vec,
      std::make_unique<::media::CdmCallbackPromise<>>(
          /*resolve_cb=*/base::BindOnce(
              success_cb, &ticket_to_new_session_promise_, ticket, session_id),
          /*reject_cb=*/base::BindOnce(
              failure_cb, &ticket_to_new_session_promise_, ticket)));
}

// Called by starboard (via CallOnSessionUpdated) once a session has been
// updated.
void StarboardDecryptorCast::OnSessionUpdated(int ticket,
                                              StarboardDrmStatus status,
                                              std::string error_message,
                                              std::string session_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  LOG(INFO) << "StarboardDecryptorCast::OnSessionUpdated, ticket: " << ticket
            << ", status: " << status << ", session id: " << session_id
            << ", error: " << error_message;
  auto it = ticket_to_simple_cdm_promise_.find(ticket);

  if (it == ticket_to_simple_cdm_promise_.end()) {
    LOG(WARNING) << "Bad ticket for DRM session update request: " << ticket;
    return;
  }

  // Regardless of success/failure, the promise at it->second must be
  // resolved/rejected and then deleted.
  if (status != kStarboardDrmStatusSuccess) {
    LOG(ERROR) << "Call to StarboardDrmUpdateSession for ticket " << it->first
               << "  failed with error: " << error_message;
    it->second->reject(StarboardDrmErrorStatusToCdmException(status), 0,
                       error_message);
  } else if (session_id.empty()) {
    LOG(ERROR)
        << "Starboard returned an empty session_id on success for ticket "
        << it->first;
    it->second->reject(::media::CdmPromise::Exception::INVALID_STATE_ERROR, 0,
                       "");
  } else {
    // Success case.
    LOG(INFO) << "Updated session id " << session_id << " for ticket "
              << it->first;
    it->second->resolve();
  }
  ticket_to_simple_cdm_promise_.erase(it);
}

// Called by starboard (via CallOnKeyStatusesChanged) when the status of keys
// change.
void StarboardDecryptorCast::OnKeyStatusesChanged(
    std::string session_id,
    std::vector<StarboardDrmKeyId> key_ids,
    std::vector<StarboardDrmKeyStatus> key_statuses) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK_EQ(key_ids.size(), key_statuses.size());

  if (key_ids.empty()) {
    LOG(ERROR) << "No keys were provided for session " << session_id;
    return;
  }

  LOG(INFO) << "StarboardDecryptorCast::OnKeyStatusChanged for "
            << key_ids.size() << " keys";
  ::media::CdmKeysInfo keys_info;
  bool usable_keys_exist = false;
  for (size_t i = 0; i < key_ids.size(); ++i) {
    const StarboardDrmKeyId& key_id = key_ids[i];
    const StarboardDrmKeyStatus status = key_statuses[i];

    const std::string key_name(
        reinterpret_cast<const char*>(&key_id.identifier),
        key_id.identifier_size);
    CHECK_GE(key_id.identifier_size, 0);
    const base::span identifier_span =
        base::span(key_id.identifier)
            .first(static_cast<size_t>(key_id.identifier_size));
    const size_t key_hash = base::FastHash(identifier_span);
    LOG(INFO) << "DRM key (hash) " << key_hash << " changed status to "
              << DrmKeyStatusToString(status) << ", for session " << session_id;

    auto key_info = std::make_unique<::media::CdmKeyInformation>();
    key_info->key_id.assign(identifier_span.begin(), identifier_span.end());
    key_info->status = ToMediaKeyStatus(status);
    keys_info.push_back(std::move(key_info));

    usable_keys_exist =
        usable_keys_exist || (status == kStarboardDrmKeyStatusUsable);

    switch (status) {
      case kStarboardDrmKeyStatusUsable:
      case kStarboardDrmKeyStatusRestricted:
      case kStarboardDrmKeyStatusDownscaled:
        // As long as the key is available to the DRM system in some way, we
        // should treat the key as available so that the MediaPipelineBackend
        // can push buffers for the given key.
        StarboardDrmKeyTracker::GetInstance().AddKey(key_name, session_id);
        break;
      case kStarboardDrmKeyStatusPending:
        // The key status will be updated later; do nothing for now.
        break;
      case kStarboardDrmKeyStatusExpired:
      case kStarboardDrmKeyStatusReleased:
      case kStarboardDrmKeyStatusError:
        // The key is no longer usable.
        StarboardDrmKeyTracker::GetInstance().RemoveKey(key_name, session_id);
        break;
    }
  }

  OnSessionKeysChange(session_id, usable_keys_exist, std::move(keys_info));
}

// Called by starboard (via CallOnCertificateUpdated) when a certificate has
// been updated.
void StarboardDecryptorCast::OnCertificateUpdated(int ticket,
                                                  StarboardDrmStatus status,
                                                  std::string error_message) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = ticket_to_simple_cdm_promise_.find(ticket);

  if (it == ticket_to_simple_cdm_promise_.end()) {
    LOG(WARNING) << "Bad ticket for DRM certificate update request: " << ticket;
    return;
  }

  // Regardless of success/failure, the promise at it->second must be
  // resolved/rejected and then deleted.
  if (status != kStarboardDrmStatusSuccess) {
    LOG(ERROR) << "Call to StarboardDrmUpdateServerCertificate for ticket "
               << it->first << "  failed with error: " << error_message;
    it->second->reject(StarboardDrmErrorStatusToCdmException(status), 0,
                       error_message);
  } else {
    // Success case.
    LOG(INFO) << "Updated DRM certificate for ticket " << it->first;
    it->second->resolve();
  }
  ticket_to_simple_cdm_promise_.erase(it);
}

// Called by starboard (via CallOnSessionClosed) when a session has closed.
void StarboardDecryptorCast::OnSessionClosed(std::string session_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  LOG(INFO) << "StarboardDecryptorCast::OnSessionClosed, session_id: "
            << session_id;

  // This must be called before any promises run. If the promise is resolved
  // before OnSessionClosed runs, MediaKeySession::Dispose gets called before
  // CdmSessionAdapter::OnSessionClosed runs, so the session is no longer
  // recognized and the JS is not notified that the session closed (in JS, the
  // MediaKeySession.closed promise is never resolved).
  //
  // In production a MojoCdm is used. Since mojo does not guarantee ordering
  // between different interfaces (ContentDecryptionModule.CloseSession and
  // ContentDecryptionModuleClient.OnSessionClosed), technically there is not a
  // guarantee that the OnSessionClosed callback will run before the promises
  // are resolved. However, in practice posting the callbacks to a separate task
  // seems to work.
  //
  // See crbug.com/402489622 and
  // https://w3c.github.io/encrypted-media/#dom-mediakeysession-close for more
  // info.
  CastCdm::OnSessionClosed(session_id, ::media::CdmSessionClosedReason::kClose);
  session_ids_.erase(session_id);
  StarboardDrmKeyTracker::GetInstance().RemoveKeysForSession(session_id);

  auto it = session_id_to_simple_cdm_promises_.find(session_id);
  if (it == session_id_to_simple_cdm_promises_.end()) {
    LOG(ERROR)
        << "Bad session ID passed to StarboardDrmCloseSession's callback: "
        << session_id;
    return;
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](std::vector<std::unique_ptr<::media::SimpleCdmPromise>> promises) {
            LOG(INFO) << "Resolving OnSessionClosed promises";
            for (std::unique_ptr<::media::SimpleCdmPromise>& promise :
                 promises) {
              promise->resolve();
            }
          },
          std::move(it->second)));

  session_id_to_simple_cdm_promises_.erase(it);
}

}  // namespace media
}  // namespace chromecast
