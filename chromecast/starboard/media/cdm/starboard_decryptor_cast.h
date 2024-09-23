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
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#include "media/base/cdm_promise.h"
#include "media/base/provision_fetcher.h"

namespace chromecast {
namespace media {

// An implementation of CastCdm that forwards calls to an underlying Starboard
// SbDrmSystem. Currently only supports widevine (the key system passed to
// starboard is hardcoded, as is the provisioning server URL).
//
// The constructor, destructor, and all functions (except the Call* callbacks
// stored in callback_handler_) must be called on the same sequence.
class StarboardDecryptorCast : public CastCdm {
 public:
  explicit StarboardDecryptorCast(
      ::media::CreateFetcherCB create_provision_fetcher_cb,
      MediaResourceTracker* media_resource_tracker);

  // Disallow copy and assign.
  StarboardDecryptorCast(const StarboardDecryptorCast&) = delete;
  StarboardDecryptorCast& operator=(const StarboardDecryptorCast&) = delete;

  // For testing purposes, `starboard` will be used to call starboard functions.
  void SetStarboardApiWrapperForTest(
      std::unique_ptr<StarboardApiWrapper> starboard);

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
                            const std::string& session_id,
                            const std::vector<uint8_t>& content);

  // Called by starboard (via CallOnSessionUpdateRequest) once a new session has
  // been created.
  void OnSessionUpdateRequest(void* drm_system,
                              int ticket,
                              StarboardDrmStatus status,
                              StarboardDrmSessionRequestType type,
                              std::optional<std::string> error_message,
                              std::optional<std::string> session_id,
                              std::vector<uint8_t> content);

  // Called by starboard (via CallOnSessionUpdated) once a session has been
  // updated.
  void OnSessionUpdated(void* drm_system,
                        int ticket,
                        StarboardDrmStatus status,
                        std::optional<std::string> error_message,
                        std::optional<std::string> session_id);

  // Called by starboard (via CallOnKeyStatusesChanged) when the status of keys
  // change.
  void OnKeyStatusesChanged(
      void* drm_system,
      std::string session_id,
      std::vector<std::pair<StarboardDrmKeyId, StarboardDrmKeyStatus>>
          key_ids_and_statuses);

  // Called by starboard (via CallOnCertificateUpdated) when a certificate has
  // been updated.
  void OnCertificateUpdated(void* drm_system,
                            int ticket,
                            StarboardDrmStatus status,
                            std::optional<std::string> error_message);

  // Called by starboard (via CallOnSessionClosed) when a session has closed.
  void OnSessionClosed(void* drm_system, std::string session_id);

  // Calls OnSessionUpdateRequest for `context`, which is an instance of
  // StarboardDecryptorCast.
  static void CallOnSessionUpdateRequest(void* drm_system,
                                         void* context,
                                         int ticket,
                                         StarboardDrmStatus status,
                                         StarboardDrmSessionRequestType type,
                                         const char* error_message,
                                         const void* session_id,
                                         int session_id_size,
                                         const void* content,
                                         int content_size,
                                         const char* url);

  // Calls OnSessionUpdated for `context`, which is an instance of
  // StarboardDecryptorCast.
  static void CallOnSessionUpdated(void* drm_system,
                                   void* context,
                                   int ticket,
                                   StarboardDrmStatus status,
                                   const char* error_message,
                                   const void* session_id,
                                   int session_id_size);

  // Calls OnKeyStatusesChanged for `context`, which is an instance of
  // StarboardDecryptorCast.
  static void CallOnKeyStatusesChanged(
      void* drm_system,
      void* context,
      const void* session_id,
      int session_id_size,
      int number_of_keys,
      const StarboardDrmKeyId* key_ids,
      const StarboardDrmKeyStatus* key_statuses);

  // Calls OnCertificateUpdated for `context`, which is an instance of
  // StarboardDecryptorCast.
  static void CallOnCertificateUpdated(void* drm_system,
                                       void* context,
                                       int ticket,
                                       StarboardDrmStatus status,
                                       const char* error_message);

  // Calls OnSessionClosed for `context`, which is an instance of
  // StarboardDecryptorCast.
  static void CallOnSessionClosed(void* drm_system,
                                  void* context,
                                  const void* session_id,
                                  int session_id_size);

  // Called when provisioning the device, in response to an individualization
  // request.
  void OnProvisionResponse(int ticket,
                           const std::string& session_id,
                           bool success,
                           const std::string& response);

  THREAD_CHECKER(thread_checker_);
  ::media::CreateFetcherCB create_provision_fetcher_cb_;
  StarboardDrmSystemCallbackHandler callback_handler_;
  std::unique_ptr<StarboardApiWrapper> starboard_;
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

  // An opaque handle to an SbDrmSystem instance.
  void* drm_system_ = nullptr;
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
