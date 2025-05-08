// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_MEDIA_CDM_STARBOARD_DECRYPTOR_CAST_H_
#define CHROMECAST_STARBOARD_MEDIA_CDM_STARBOARD_DECRYPTOR_CAST_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "chromecast/media/cdm/cast_cdm.h"
#include "chromecast/starboard/media/cdm/starboard_drm_wrapper.h"
#include "media/base/cdm_promise.h"
#include "media/base/provision_fetcher.h"

namespace chromecast {
namespace media {

// An implementation of CastCdm that forwards calls to an underlying Starboard
// SbDrmSystem. Currently only supports widevine (the key system passed to
// starboard is hardcoded, as is the provisioning server URL).
//
// Calls to starboard go through the StarboardDrmWrapper singleton.
//
// The constructor, destructor, and all functions must be called on the same
// sequence.
class StarboardDecryptorCast : public CastCdm,
                               public StarboardDrmWrapper::Client {
 public:
  explicit StarboardDecryptorCast(
      ::media::CreateFetcherCB create_provision_fetcher_cb,
      MediaResourceTracker* media_resource_tracker);

  // Disallow copy and assign.
  StarboardDecryptorCast(const StarboardDecryptorCast&) = delete;
  StarboardDecryptorCast& operator=(const StarboardDecryptorCast&) = delete;

  // ::media::ContentDecryptionModule implementation:
  void CreateSessionAndGenerateRequest(
      ::media::CdmSessionType session_type,
      ::media::EmeInitDataType init_data_type,
      const std::vector<uint8_t>& init_data,
      std::unique_ptr<::media::NewSessionCdmPromise> promise) override;
  void LoadSession(
      ::media::CdmSessionType session_type,
      const std::string& session_id,
      std::unique_ptr<::media::NewSessionCdmPromise> promise) override;
  void UpdateSession(
      const std::string& web_session_id,
      const std::vector<uint8_t>& response,
      std::unique_ptr<::media::SimpleCdmPromise> promise) override;
  void CloseSession(
      const std::string& web_session_id,
      std::unique_ptr<::media::SimpleCdmPromise> promise) override;
  void RemoveSession(
      const std::string& session_id,
      std::unique_ptr<::media::SimpleCdmPromise> promise) override;
  void SetServerCertificate(
      const std::vector<uint8_t>& certificate_data,
      std::unique_ptr<::media::SimpleCdmPromise> promise) override;

  // CastCdm implementation:
  std::unique_ptr<DecryptContextImpl> GetDecryptContext(
      const std::string& key_id,
      EncryptionScheme encryption_scheme) const override;
  void SetKeyStatus(const std::string& key_id,
                    CastKeyStatus key_status,
                    uint32_t system_code) override;
  void SetVideoResolution(int width, int height) override;

 private:
  using SessionRequest = std::pair<
      base::OnceCallback<void(std::unique_ptr<::media::NewSessionCdmPromise>)>,
      std::unique_ptr<::media::NewSessionCdmPromise>>;

  ~StarboardDecryptorCast() override;

  // CastCdm implementation:
  void InitializeInternal() override;

  // Sends the actual request to create the session.
  void HandleCreateSessionAndGenerateRequest(
      ::media::CdmSessionType session_type,
      ::media::EmeInitDataType init_data_type,
      const std::vector<uint8_t>& init_data,
      std::unique_ptr<::media::NewSessionCdmPromise> promise);

  // Rejects all promises in queued_session_requests_.
  void RejectPendingPromises();

  // Processes any pending requests to create sessions.
  void ProcessQueuedSessionRequests();

  // Sends a provision request to the Widevine licensing server. This is used to
  // create cert.bin in starboard. Note that -- for the gLinux implementation of
  // starboard, at least -- cert.bin is not an actual file. It's a
  // logical/conceptual file, and its stored in the cache at
  // ~/.cache/cobalt/wvcdm.dat.
  void SendProvisionRequest(int ticket,
                            std::string session_id,
                            const std::string& content);

  // StarboardDrmWrapper::Client implementation:
  void OnSessionUpdateRequest(int ticket,
                              StarboardDrmStatus status,
                              StarboardDrmSessionRequestType type,
                              std::string error_message,
                              std::string session_id,
                              std::vector<uint8_t> content) override;
  void OnSessionUpdated(int ticket,
                        StarboardDrmStatus status,
                        std::string error_message,
                        std::string session_id) override;
  void OnKeyStatusesChanged(
      std::string session_id,
      std::vector<StarboardDrmKeyId> key_ids,
      std::vector<StarboardDrmKeyStatus> key_statuses) override;
  void OnCertificateUpdated(int ticket,
                            StarboardDrmStatus status,
                            std::string error_message) override;
  void OnSessionClosed(std::string session_id) override;

  // Called when provisioning the device, in response to an individualization
  // request.
  void OnProvisionResponse(int ticket,
                           const std::string& session_id,
                           bool success,
                           const std::string& response);

  THREAD_CHECKER(thread_checker_);
  ::media::CreateFetcherCB create_provision_fetcher_cb_;
  bool server_certificate_updatable_ = false;
  std::queue<SessionRequest> queued_session_requests_;

  // Ticket used to track session update requests.
  int current_ticket_ = 0;
  base::flat_map<int, std::unique_ptr<::media::NewSessionCdmPromise>>
      ticket_to_new_session_promise_;
  base::flat_map<int, std::unique_ptr<::media::SimpleCdmPromise>>
      ticket_to_simple_cdm_promise_;
  base::flat_map<std::string,
                 std::vector<std::unique_ptr<::media::SimpleCdmPromise>>>
      session_id_to_simple_cdm_promises_;

  // There's a session create/load in processing. If true, all the new session
  // create/load operations should be queued.
  bool pending_session_setup_ = false;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<::media::ProvisionFetcher> provision_fetcher_;

  // Tracks all sessions that have had at least one key created. Used to clean
  // up keys in StarboardDrmKeyTracker on destruction.
  base::flat_set<std::string> session_ids_;

  // This must be destructed first, to invalidate any remaining weak ptrs.
  base::WeakPtrFactory<StarboardDecryptorCast> weak_factory_{this};
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_MEDIA_CDM_STARBOARD_DECRYPTOR_CAST_H_
