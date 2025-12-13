// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/cdm/starboard_drm_wrapper.h"

#include "base/at_exit.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chromecast/common/timing_tracker.h"
#include "chromecast/starboard/chromecast/starboard_adapter/public/cast_starboard_api_adapter.h"

namespace chromecast {
namespace media {

namespace {

// For tests, this needs to be set/cleared in each test.
StarboardDrmWrapper* g_test_drm_wrapper = nullptr;

}  // namespace

StarboardDrmWrapper::DrmSystemResource::DrmSystemResource() {
  StarboardDrmWrapper::GetInstance().AddResource(this);
}

StarboardDrmWrapper::DrmSystemResource::~DrmSystemResource() {
  StarboardDrmWrapper::GetInstance().RemoveResource(this);
}

StarboardDrmWrapper::Client::Client() {
  StarboardDrmWrapper::GetInstance().AddClient(this);
}

StarboardDrmWrapper::Client::~Client() {
  StarboardDrmWrapper::GetInstance().RemoveClient(this);
}

bool StarboardDrmWrapper::HasClients() {
  CHECK(task_runner_->RunsTasksInCurrentSequence());

  return !clients_.empty();
}

void StarboardDrmWrapper::AddClient(Client* client) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());

  LOG(INFO) << "StarboardDrmWrapper::AddClient(client=" << client << ")";

  const auto& [unused_it, inserted] = clients_.insert(client);
  if (!inserted) {
    LOG(WARNING) << "Duplicate Client* inserted";
  }
}

void StarboardDrmWrapper::RemoveClient(Client* client) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());

  LOG(INFO) << "StarboardDrmWrapper::RemoveClient(client=" << client << ")";

  // Update session_id_to_client_.
  {
    auto it = session_id_to_client_.begin();
    while (it != session_id_to_client_.end()) {
      if (it->second == client) {
        LOG(INFO) << "Removing session_id=" << it->first
                  << " for client=" << client;
        it = session_id_to_client_.erase(it);
      } else {
        ++it;
      }
    }
  }

  // Update ticket_to_client_.
  {
    auto it = ticket_to_client_.begin();
    while (it != ticket_to_client_.end()) {
      if (it->second == client) {
        LOG(INFO) << "Removing internal ticket=" << it->first
                  << " for client=" << client;
        it = ticket_to_client_.erase(it);
      } else {
        ++it;
      }
    }
  }

  // Update clients_.
  clients_.erase(client);
}

void StarboardDrmWrapper::CallOnSessionUpdateRequest(
    void* drm_system,
    void* context,
    int ticket,
    StarboardDrmStatus status,
    StarboardDrmSessionRequestType type,
    std::string error_message,
    std::string session_id,
    std::vector<uint8_t> content,
    std::string url) {
  reinterpret_cast<StarboardDrmWrapper*>(context)->OnSessionUpdateRequest(
      ticket, status, type, std::move(error_message), std::move(session_id),
      std::move(content));
}

void StarboardDrmWrapper::CallOnSessionUpdated(void* drm_system,
                                               void* context,
                                               int ticket,
                                               StarboardDrmStatus status,
                                               std::string error_message,
                                               std::string session_id) {
  reinterpret_cast<StarboardDrmWrapper*>(context)->OnSessionUpdated(
      ticket, status, std::move(error_message), std::move(session_id));
}

void StarboardDrmWrapper::CallOnKeyStatusesChanged(
    void* drm_system,
    void* context,
    std::string session_id,
    std::vector<StarboardDrmKeyId> key_ids,
    std::vector<StarboardDrmKeyStatus> key_statuses) {
  reinterpret_cast<StarboardDrmWrapper*>(context)->OnKeyStatusesChanged(
      std::move(session_id), std::move(key_ids), std::move(key_statuses));
}

void StarboardDrmWrapper::CallOnServerCertificateUpdated(
    void* drm_system,
    void* context,
    int ticket,
    StarboardDrmStatus status,
    std::string error_message) {
  reinterpret_cast<StarboardDrmWrapper*>(context)->OnServerCertificateUpdated(
      ticket, status, std::move(error_message));
}

void StarboardDrmWrapper::CallOnSessionClosed(void* drm_system,
                                              void* context,
                                              std::string session_id) {
  reinterpret_cast<StarboardDrmWrapper*>(context)->OnSessionClosed(
      std::move(session_id));
}

void StarboardDrmWrapper::OnSessionUpdateRequest(
    int ticket,
    StarboardDrmStatus status,
    StarboardDrmSessionRequestType type,
    std::string error_message,
    std::string session_id,
    std::vector<uint8_t> content) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    // base::Unretained is safe here because this class is a singleton.
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&StarboardDrmWrapper::OnSessionUpdateRequest,
                                  base::Unretained(this), ticket, status, type,
                                  std::move(error_message),
                                  std::move(session_id), std::move(content)));
    return;
  }

  LOG(INFO) << "StarboardDrmWrapper::OnSessionUpdateRequest(ticket=" << ticket
            << ", status=" << status << ", type=" << type
            << ", error_message=" << error_message
            << ", session_id=" << session_id << ")";

  Client* client = FindClient(ticket, session_id);
  if (client == nullptr) {
    LOG(ERROR) << "OnSessionUpdateRequest failed to find client for internal "
                  "DRM ticket="
               << ticket << ", session_id=" << session_id;
    return;
  }

  auto it_and_was_inserted = session_id_to_client_.insert({session_id, client});
  auto it = it_and_was_inserted.first;
  const bool was_inserted = it_and_was_inserted.second;

  if (!was_inserted) {
    // This is normal, e.g. during provisioning. We may want to add a check if
    // the client is different from the one in the map, though.
    LOG(INFO) << "Client already exists for session ID=" << session_id
              << ". Existing client=" << it->second
              << ". New client=" << client;
  }

  client->OnSessionUpdateRequest(FindClientTicket(ticket), status, type,
                                 std::move(error_message),
                                 std::move(session_id), std::move(content));
}

void StarboardDrmWrapper::OnSessionUpdated(int ticket,
                                           StarboardDrmStatus status,
                                           std::string error_message,
                                           std::string session_id) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    // base::Unretained is safe here because this class is a singleton.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&StarboardDrmWrapper::OnSessionUpdated,
                       base::Unretained(this), ticket, status,
                       std::move(error_message), std::move(session_id)));
    return;
  }

  LOG(INFO) << "StarboardDrmWrapper::OnSessionUpdated(ticket=" << ticket
            << ", status=" << status << ", error_message=" << error_message
            << ", session_id=" << session_id << ")";

  Client* client = FindClient(ticket, session_id);
  if (client == nullptr) {
    LOG(ERROR)
        << "OnSessionUpdated failed to find client for internal DRM ticket="
        << ticket << ", session_id=" << session_id;
    return;
  }

  client->OnSessionUpdated(FindClientTicket(ticket), status,
                           std::move(error_message), std::move(session_id));
}

void StarboardDrmWrapper::OnKeyStatusesChanged(
    std::string session_id,
    std::vector<StarboardDrmKeyId> key_ids,
    std::vector<StarboardDrmKeyStatus> key_statuses) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    // base::Unretained is safe here because this class is a singleton.
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&StarboardDrmWrapper::OnKeyStatusesChanged,
                                  base::Unretained(this), std::move(session_id),
                                  std::move(key_ids), std::move(key_statuses)));
    return;
  }

  LOG(INFO) << "StarboardDrmWrapper::OnKeyStatusesChanged(session_id="
            << session_id << ", num_keys=" << key_ids.size() << ")";

  Client* client = FindClient(/*ticket=*/kInvalidTicket, session_id);
  if (client == nullptr) {
    LOG(ERROR) << "OnKeyStatusesChanged failed to find client for session_id="
               << session_id;
    return;
  }
  client->OnKeyStatusesChanged(session_id, std::move(key_ids),
                               std::move(key_statuses));
}

void StarboardDrmWrapper::OnServerCertificateUpdated(
    int ticket,
    StarboardDrmStatus status,
    std::string error_message) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    // base::Unretained is safe here because this class is a singleton.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&StarboardDrmWrapper::OnServerCertificateUpdated,
                       base::Unretained(this), ticket, status,
                       std::move(error_message)));
    return;
  }

  LOG(INFO) << "StarboardDrmWrapper::OnServerCertificateUpdated(ticket="
            << ticket << ", status=" << status
            << ", error_message=" << error_message << ")";

  Client* client = FindClient(ticket, /*session_id=*/"");
  if (client == nullptr) {
    LOG(ERROR) << "OnCertificateUpdated failed to find client for internal "
                  "DRM ticket="
               << ticket;
    return;
  }
  client->OnCertificateUpdated(FindClientTicket(ticket), status,
                               std::move(error_message));
}

void StarboardDrmWrapper::OnSessionClosed(std::string session_id) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    // base::Unretained is safe here because this class is a singleton.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&StarboardDrmWrapper::OnSessionClosed,
                       base::Unretained(this), std::move(session_id)));
    return;
  }

  LOG(INFO) << "StarboardDrmWrapper::OnSessionClosed(session_id=" << session_id
            << ")";

  auto it = session_id_to_client_.find(session_id);
  if (it == session_id_to_client_.end()) {
    LOG(ERROR) << "OnSessionClosed failed to find client for session_id="
               << session_id;
    return;
  }
  it->second->OnSessionClosed(std::move(session_id));

  LOG(INFO) << "Removing mapping from session_id=" << it->first
            << " to client=" << it->second;
  // Remove the session ID mapping, since the session is closed.
  session_id_to_client_.erase(it);
}

StarboardDrmWrapper::Client* StarboardDrmWrapper::FindClient(
    int internal_ticket,
    const std::string& session_id) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());

  auto ticket_it = ticket_to_client_.find(internal_ticket);
  if (ticket_it != ticket_to_client_.end()) {
    Client* client = ticket_it->second;
    LOG(INFO) << "Found client=" << client
              << " for internal ticket=" << internal_ticket;
    ticket_to_client_.erase(ticket_it);
    return client;
  }

  auto session_it = session_id_to_client_.find(session_id);
  if (session_it != session_id_to_client_.end()) {
    LOG(INFO) << "Found client=" << session_it->second
              << " for session=" << session_id;
    return session_it->second;
  }
  return nullptr;
}

int StarboardDrmWrapper::FindClientTicket(int internal_ticket) {
  auto it = ticket_map_.find(internal_ticket);
  if (it == ticket_map_.end()) {
    return kInvalidTicket;
  }
  const int client_ticket = it->second;
  ticket_map_.erase(it);

  LOG(INFO) << "Internal ticket=" << internal_ticket
            << " maps to client ticket=" << client_ticket;
  return client_ticket;
}

void StarboardDrmWrapper::GenerateSessionUpdateRequest(
    Client* client,
    int ticket,
    const std::string& type,
    const std::vector<uint8_t>& init_data) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());

  const int internal_ticket = GetNextTicket();
  ticket_map_[internal_ticket] = ticket;
  ticket_to_client_[internal_ticket] = client;

  starboard_->DrmGenerateSessionUpdateRequest(drm_system_, internal_ticket,
                                              type.c_str(), init_data.data(),
                                              init_data.size());
}

void StarboardDrmWrapper::UpdateSession(Client* client,
                                        int ticket,
                                        const std::string& session_id,
                                        const std::vector<uint8_t>& key) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());

  const int internal_ticket = GetNextTicket();
  ticket_map_[internal_ticket] = ticket;
  ticket_to_client_[internal_ticket] = client;

  // This will eventually call OnSessionUpdated.
  starboard_->DrmUpdateSession(drm_system_, internal_ticket, key.data(),
                               key.size(), session_id.c_str(),
                               session_id.size());
}

void StarboardDrmWrapper::CloseSession(Client* client,
                                       const std::string& session_id) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());

  auto it_and_inserted = session_id_to_client_.insert({session_id, client});
  auto it = it_and_inserted.first;
  const bool inserted = it_and_inserted.second;

  // If a session is closing, it is expected that we were already tracking that
  // session and its client. This condition checks whether the insertion
  // occurred, meaning we were NOT tracking the session.
  if (inserted) {
    LOG(WARNING) << "Closing session_id=" << session_id
                 << ", which was not being tracked";
  }

  if (it->second != client) {
    LOG(WARNING) << "client=" << client
                 << " is closing session_id=" << session_id
                 << ", which was created for client=" << it->second;
  }
  starboard_->DrmCloseSession(drm_system_, session_id.c_str(),
                              session_id.size());
}

bool StarboardDrmWrapper::IsServerCertificateUpdatable() {
  CHECK(task_runner_->RunsTasksInCurrentSequence());

  return starboard_->DrmIsServerCertificateUpdatable(drm_system_);
}

void StarboardDrmWrapper::UpdateServerCertificate(
    Client* client,
    int ticket,
    const std::vector<uint8_t>& certificate_data) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());

  const int internal_ticket = GetNextTicket();
  ticket_map_[internal_ticket] = ticket;
  ticket_to_client_[internal_ticket] = client;

  starboard_->DrmUpdateServerCertificate(drm_system_, internal_ticket,
                                         certificate_data.data(),
                                         certificate_data.size());
}

int StarboardDrmWrapper::GetNextTicket() {
  CHECK(task_runner_->RunsTasksInCurrentSequence());

  const int ticket = ticket_++;
  if (ticket_ == kInvalidTicket) {
    ++ticket_;
  }
  return ticket;
}

StarboardDrmWrapper::StarboardDrmWrapper()
    : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  // This is the version of the constructor that runs in production. As such, we
  // should properly destroy the SbDrmSystem via an AtExitManager.
  //
  // Use of base::Unretained is safe because this object should never be
  // destructed in production code (it's a private destructor, and the only code
  // that destroys it is the test-only function SetSingletonForTesting).
  if (auto* starboard_api_adapter =
          chromecast::CastStarboardApiAdapter::GetInstance();
      starboard_api_adapter != nullptr) {
    LOG(INFO) << "Subscribing to CastStarboardApiAdapter. this=" << this;
    starboard_api_adapter->Subscribe(this, nullptr);
  } else {
    LOG(WARNING) << "CastStarboardApiAdapter::GetInstance() returned null. "
                    "This should only happen in tests.";
  }

  base::AtExitManager::RegisterTask(base::BindOnce(
      &StarboardDrmWrapper::MaybeDestroySbDrmSystem, base::Unretained(this)));
  owned_starboard_ = GetStarboardApiWrapper();
  starboard_ = owned_starboard_.get();
  CHECK(starboard_->EnsureInitialized()) << "Failed to initialize starboard";

  CHROMECAST_TIMING_TRACKER;
  drm_system_ = starboard_->CreateDrmSystem(
      /*key_system=*/"com.widevine.alpha",
      /*callback_handler=*/&callback_handler_);
  CHECK(drm_system_) << "Failed to create an SbDrmSystem";
  LOG(INFO) << "Created DRM system with address " << drm_system_;
}

StarboardDrmWrapper::StarboardDrmWrapper(StarboardApiWrapper* starboard)
    : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  CHECK(starboard);
  starboard_ = starboard;
  drm_system_ = starboard_->CreateDrmSystem(
      /*key_system=*/"com.widevine.alpha",
      /*callback_handler=*/&callback_handler_);
  CHECK(drm_system_) << "Failed to create an SbDrmSystem";
  LOG(INFO) << "Created DRM system with address " << drm_system_;
}

StarboardDrmWrapper::~StarboardDrmWrapper() = default;

void StarboardDrmWrapper::AddResource(DrmSystemResource* resource) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    // base::Unretained is safe because this class is a singleton.
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&StarboardDrmWrapper::AddResource,
                                          base::Unretained(this), resource));
    return;
  }
  resources_.insert(resource);
}

void StarboardDrmWrapper::RemoveResource(DrmSystemResource* resource) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    // base::Unretained is safe because this class is a singleton.
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&StarboardDrmWrapper::RemoveResource,
                                          base::Unretained(this), resource));
    return;
  }
  resources_.erase(resource);

  if (resources_.empty() && cast_exiting_) {
    LOG(INFO)
        << "Retrying MaybeDestroySbDrmSystem because the last resource was "
           "removed.";
    MaybeDestroySbDrmSystem();
  }
}

void StarboardDrmWrapper::MaybeDestroySbDrmSystem() {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    // base::Unretained is safe because this class is a singleton.
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&StarboardDrmWrapper::MaybeDestroySbDrmSystem,
                                  base::Unretained(this)));
    return;
  }

  cast_exiting_ = true;
  if (!resources_.empty()) {
    LOG(INFO) << "Delaying destruction of SbDrmSystem because there are still "
              << resources_.size() << " resources held.";
    return;
  }

  LOG(INFO) << "Destroying SbDrmSystem because core_runtime is shutting down.";
  starboard_->DrmDestroySystem(drm_system_);

  if (auto* starboard_api_adapter =
          chromecast::CastStarboardApiAdapter::GetInstance();
      starboard_api_adapter != nullptr) {
    LOG(INFO) << "Unsubscribing from CastStarboardApiAdapter. this=" << this;
    starboard_api_adapter->Unsubscribe(this);
  } else {
    LOG(WARNING) << "CastStarboardApiAdapter::GetInstance() returned null. "
                    "This should only happen in tests.";
  }

  // We need to destroy owned_starboard_ here, so that it unsubscribes from
  // CastStarboardApiAdapter.
  owned_starboard_ = nullptr;
  starboard_ = nullptr;
  // TODO(antoniori): maybe add checks in other places to ensure that we crash
  // if attempting to use starboard_ after this point.
}

StarboardDrmWrapper& StarboardDrmWrapper::GetInstance() {
  if (g_test_drm_wrapper) {
    return *g_test_drm_wrapper;
  }

  static base::NoDestructor<StarboardDrmWrapper> instance;
  return *instance;
}

void* StarboardDrmWrapper::GetDrmSystem() {
  return drm_system_;
}

void StarboardDrmWrapper::SetSingletonForTesting(
    StarboardApiWrapper* starboard) {
  LOG(INFO) << "Overriding StarboardDrmWrapper singleton for testing";
  if (g_test_drm_wrapper) {
    delete g_test_drm_wrapper;
  }
  g_test_drm_wrapper = new StarboardDrmWrapper(starboard);
}

}  // namespace media
}  // namespace chromecast
