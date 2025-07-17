// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_MEDIA_CDM_STARBOARD_DRM_WRAPPER_H_
#define CHROMECAST_STARBOARD_MEDIA_CDM_STARBOARD_DRM_WRAPPER_H_

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"

namespace chromecast {
namespace media {

// A wrapper around a single SbDrmSystem instance. This class supports
// multiplexing calls from multiple callers (Clients).
//
// To prevent the destruction of the underlying SbDrmSystem, classes can
// instantiate an StarboardDrmWrapper::DrmSystemResource. The SbDrmSystem will
// not be destructed until all DrmSystemResources have been destructed. Note
// that Clients automatically hold a DrmSystemResource.
//
// This class must only be accessed on a single sequence. It is expected to be
// used as a singleton, via GetInstance(). Its destructor should not run in
// production code.
class StarboardDrmWrapper {
 public:
  // This must match the definition of kSbDrmTicketInvalid in starboard/drm.h.
  static constexpr int kInvalidTicket = std::numeric_limits<int>::min();

  // Blocks SbDrmSystem destruction. In other words, users of
  // StarboardDrmWrapper can hold an instance of this class to guarantee that
  // the underlying SbDrmSystem has not been destructed.
  class DrmSystemResource {
   public:
    DrmSystemResource();
    ~DrmSystemResource();
  };

  // A client that interacts with a StarboardDrmWrapper to make DRM-related
  // calls into starboard. This should only be subclassed by CDM
  // implementations.
  class Client {
   public:
    Client();
    virtual ~Client();

    // Called when a new session has been created.
    virtual void OnSessionUpdateRequest(int ticket,
                                        StarboardDrmStatus status,
                                        StarboardDrmSessionRequestType type,
                                        std::string error_message,
                                        std::string session_id,
                                        std::vector<uint8_t> content) = 0;

    // Called once a session has been updated.
    virtual void OnSessionUpdated(int ticket,
                                  StarboardDrmStatus status,
                                  std::string error_message,
                                  std::string session_id) = 0;

    // Called when the status of keys change.
    virtual void OnKeyStatusesChanged(
        std::string session_id,
        std::vector<StarboardDrmKeyId> key_ids,
        std::vector<StarboardDrmKeyStatus> key_statuses) = 0;

    // Called when a certificate has been updated.
    virtual void OnCertificateUpdated(int ticket,
                                      StarboardDrmStatus status,
                                      std::string error_message) = 0;

    // Called when a session has closed.
    virtual void OnSessionClosed(std::string session_id) = 0;

   private:
    // Prevent the SbDrmSystem from being destructed while any client exists.
    DrmSystemResource drm_resource_;
  };

  // Returns a handle to the SbDrmSystem singleton. All calls to starboard for
  // DRM should go through this.
  static StarboardDrmWrapper& GetInstance();

  // Disallow copy and assign.
  StarboardDrmWrapper(const StarboardDrmWrapper&) = delete;
  StarboardDrmWrapper& operator=(const StarboardDrmWrapper&) = delete;

  // Returns true if at least one client is registered with StarboardDrmWrapper.
  // This can be used as a signal that a CDM exists.
  bool HasClients();

  // Returns the handle to the SbDrmSystem. This should only be called for the
  // purpose of creating the SbPlayer. Any calls to SbDrmSystem should go
  // through GetInstance().
  void* GetDrmSystem();

  // Tells starboard to generate a session update request (e.g. a license
  // request).
  void GenerateSessionUpdateRequest(Client* client,
                                    int ticket,
                                    const std::string& type,
                                    const std::vector<uint8_t>& init_data);

  // Updates a session in starboard, e.g. making a key from the license server
  // available for a given session.
  void UpdateSession(Client* client,
                     int ticket,
                     const std::string& session_id,
                     const std::vector<uint8_t>& key);

  // Closes the specified session.
  void CloseSession(Client* client, const std::string& session_id);

  // Updates the server certificate. This should only be called if
  // IsServerCertificateUpdatable() returns true.
  void UpdateServerCertificate(Client* client,
                               int ticket,
                               const std::vector<uint8_t>& certificate_data);

  // Returns whether the server certificate can be updated.
  bool IsServerCertificateUpdatable();

  static void SetSingletonForTesting(StarboardApiWrapper* starboard);

 private:
  friend base::NoDestructor<StarboardDrmWrapper>;
  friend class StarboardDrmWrapperTestPeer;

  StarboardDrmWrapper();

  // Test-only constructor. Used in SetSingletonForTesting.
  explicit StarboardDrmWrapper(StarboardApiWrapper* starboard);

  virtual ~StarboardDrmWrapper();

  // Starts tracking `resource`. Destruction of the SbDrmSystem is prevented
  // until all resources have been removed via RemoveResource.
  void AddResource(DrmSystemResource* resource);

  // Stops tracking `resource`. Destruction of the SbDrmSystem is prevented
  // until all resources have been removed via RemoveResource.
  void RemoveResource(DrmSystemResource* resource);

  // Destroys the owned SbDrmSystem and unsubscribes from
  // CastStarboardApiAdapter. This should only be called when the cast runtime
  // is stopping.
  void MaybeDestroySbDrmSystem();

  // Returns the next internal ticket. Avoids returning kSbDrmTicketInvalid.
  // Must be called on task_runner_.
  int GetNextTicket();

  // Called by a Client upon construction. This tells StarboardDrmWrapper that a
  // CDM instance currently exists.
  void AddClient(Client* client);

  // Called by a Client upon deletion. This tells StarboardDrmWrapper that it
  // can clean up any mappings to/from that client in its internal data
  // structures. Must be called on task_runner_.
  void RemoveClient(Client* client);

  // Attempts to find a client by `internal_ticket`. If that fails, attempts to
  // find a client by `session_id`. Returns null if a client was not found.
  Client* FindClient(int internal_ticket, const std::string& session_id);

  // Finds a client's ticket by looking up the mapping from `internal_ticket` ->
  // client ticket.
  int FindClientTicket(int internal_ticket);

  // These functions are called by starboard. In particular, a function F() is
  // called via starboard calling the static function CallF() (defined below).
  // These functions can be called on any sequence; they will post a task that
  // runs on task_runner_ if necessary.
  //
  // See the documentation in starboard/drm.h for more information. These
  // functions replace some C concepts with corresponding C++ ones for
  // convenience/safety (e.g. replacing ptr + size with a vector).
  void OnSessionUpdateRequest(int ticket,
                              StarboardDrmStatus status,
                              StarboardDrmSessionRequestType type,
                              std::string error_message,
                              std::string session_id,
                              std::vector<uint8_t> content);
  void OnSessionUpdated(int ticket,
                        StarboardDrmStatus status,
                        std::string error_message,
                        std::string session_id);
  void OnKeyStatusesChanged(std::string session_id,
                            std::vector<StarboardDrmKeyId> key_ids,
                            std::vector<StarboardDrmKeyStatus> key_statuses);
  void OnServerCertificateUpdated(int ticket,
                                  StarboardDrmStatus status,
                                  std::string error_message);
  void OnSessionClosed(std::string session_id);

  // These functions are called directly as callbacks from starboard. See the
  // documentation in starboard/drm.h for more info.
  static void CallOnSessionUpdateRequest(void* drm_system,
                                         void* context,
                                         int ticket,
                                         StarboardDrmStatus status,
                                         StarboardDrmSessionRequestType type,
                                         std::string error_message,
                                         std::string session_id,
                                         std::vector<uint8_t> content,
                                         std::string url);
  static void CallOnSessionUpdated(void* drm_system,
                                   void* context,
                                   int ticket,
                                   StarboardDrmStatus status,
                                   std::string error_message,
                                   std::string session_id);
  static void CallOnKeyStatusesChanged(
      void* drm_system,
      void* context,
      std::string session_id,
      std::vector<StarboardDrmKeyId> key_ids,
      std::vector<StarboardDrmKeyStatus> key_statuses);
  static void CallOnServerCertificateUpdated(void* drm_system,
                                             void* context,
                                             int ticket,
                                             StarboardDrmStatus status,
                                             std::string error_message);
  static void CallOnSessionClosed(void* drm_system,
                                  void* context,
                                  std::string session_id);

  // This gets passed to starboard, and tells starboard which functions to call
  // for DRM callbacks.
  StarboardDrmSystemCallbackHandler callback_handler_{
      this,
      &CallOnSessionUpdateRequest,
      &CallOnSessionUpdated,
      &CallOnKeyStatusesChanged,
      &CallOnServerCertificateUpdated,
      &CallOnSessionClosed};

  // Keeps track of session IDs and the client associated with each.
  base::flat_map<std::string, Client*> session_id_to_client_;

  // Keeps track of internal tickets (from ticket_) and the Client they
  // correspond to.
  base::flat_map<int, Client*> ticket_to_client_;

  // Tracks any existing clients, so we know whether a CDM exists.
  base::flat_set<Client*> clients_;

  // Maps from our internal ticket (ticket_) to the ticket passed in by the
  // client. This is done in case multiple Clients interact with
  // StarboardDrmWrapper, and they happen to pass the same ticket. Starboard
  // requires that tickets be unique, otherwise there can be undefined behavior.
  //
  // Guarded by ticket_map_lock_, since this may be accessed on multiple
  // sequences (Starboard's callbacks do not provide sequencing guarantees).
  base::flat_map<int, int> ticket_map_;

  // SbDrmSystem will not be destructed while this set is non-empty.
  base::flat_set<DrmSystemResource*> resources_;

  // Per the documentation at starboard/drm.h, the ticket must not be INT_MIN.
  // This is an internal ticket, not to be confused with client tickets. This
  // allows us to handle multiple clients simultaneously. For example, two
  // clients may each use the same ticket, but this unique ticket allows us to
  // identify the correct response from starboard.
  int ticket_ = 0;

  // Pointer to the SbDrmSystem instance.
  void* drm_system_ = nullptr;

  // If this is true, it means cast is exiting and we should destroy drm_system_
  // once there are no remaining DrmSystemResources.
  bool cast_exiting_ = false;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // In production this points to the object owned by owned_starboard_. For
  // tests, this may point to a mock (set via SetStarboardForTesting).
  StarboardApiWrapper* starboard_ = nullptr;

  // In production this will be populated with an owned instance of a
  // StarboardApiWrapper. For tests, a mock may be used instead, and starboard_
  // will point to that.
  std::unique_ptr<StarboardApiWrapper> owned_starboard_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_MEDIA_CDM_STARBOARD_DRM_WRAPPER_H_
