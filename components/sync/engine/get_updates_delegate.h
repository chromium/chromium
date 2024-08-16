// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_GET_UPDATES_DELEGATE_H_
#define COMPONENTS_SYNC_ENGINE_GET_UPDATES_DELEGATE_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "components/sync/engine/cycle/nudge_tracker.h"
#include "components/sync/engine/cycle/status_controller.h"
#include "components/sync/engine/data_type_registry.h"
#include "components/sync/engine/events/protocol_event.h"

namespace sync_pb {
class GetUpdatesMessage;
class ClientToServerMessage;
enum SyncEnums_GetUpdatesOrigin : int;
}  // namespace sync_pb

namespace syncer {

// Interface for GetUpdates functionality that depends on the requested
// GetUpdate type (normal, configuration, poll).  The GetUpdatesProcessor is
// given an appropriate GetUpdatesDelegate to handle type specific functionality
// on construction.
class GetUpdatesDelegate {
 public:
  GetUpdatesDelegate() = default;

  GetUpdatesDelegate(const GetUpdatesDelegate&) = delete;
  GetUpdatesDelegate& operator=(const GetUpdatesDelegate&) = delete;

  virtual ~GetUpdatesDelegate() = default;

  // Populates GetUpdate message fields that depend on GetUpdates request type.
  virtual void HelpPopulateGuMessage(
      sync_pb::GetUpdatesMessage* get_updates) const = 0;

  virtual std::unique_ptr<ProtocolEvent> GetNetworkRequestEvent(
      base::Time timestamp,
      const sync_pb::ClientToServerMessage& request) const = 0;

  virtual bool IsNotificationInfoRequired() const = 0;
};

// Functionality specific to the normal GetUpdate request.
class NormalGetUpdatesDelegate : public GetUpdatesDelegate {
 public:
  explicit NormalGetUpdatesDelegate(const NudgeTracker& nudge_tracker);

  NormalGetUpdatesDelegate(const NormalGetUpdatesDelegate&) = delete;
  NormalGetUpdatesDelegate& operator=(const NormalGetUpdatesDelegate&) = delete;

  ~NormalGetUpdatesDelegate() override;

  // Uses the member NudgeTracker to populate some fields of this GU message.
  void HelpPopulateGuMessage(
      sync_pb::GetUpdatesMessage* get_updates) const override;

  std::unique_ptr<ProtocolEvent> GetNetworkRequestEvent(
      base::Time timestamp,
      const sync_pb::ClientToServerMessage& request) const override;

  bool IsNotificationInfoRequired() const override;

 private:
  const raw_ref<const NudgeTracker> nudge_tracker_;
};

// Functionality specific to the configure GetUpdate request.
class ConfigureGetUpdatesDelegate : public GetUpdatesDelegate {
 public:
  explicit ConfigureGetUpdatesDelegate(
      sync_pb::SyncEnums_GetUpdatesOrigin origin);

  ConfigureGetUpdatesDelegate(const ConfigureGetUpdatesDelegate&) = delete;
  ConfigureGetUpdatesDelegate& operator=(const ConfigureGetUpdatesDelegate&) =
      delete;

  ~ConfigureGetUpdatesDelegate() override;

  // Sets the 'source' and 'origin' fields for this request.
  void HelpPopulateGuMessage(
      sync_pb::GetUpdatesMessage* get_updates) const override;

  std::unique_ptr<ProtocolEvent> GetNetworkRequestEvent(
      base::Time timestamp,
      const sync_pb::ClientToServerMessage& request) const override;

  bool IsNotificationInfoRequired() const override;

 private:
  const sync_pb::SyncEnums_GetUpdatesOrigin origin_;
};

// Functionality specific to the poll GetUpdate request.
class PollGetUpdatesDelegate : public GetUpdatesDelegate {
 public:
  PollGetUpdatesDelegate();

  PollGetUpdatesDelegate(const PollGetUpdatesDelegate&) = delete;
  PollGetUpdatesDelegate& operator=(const PollGetUpdatesDelegate&) = delete;

  ~PollGetUpdatesDelegate() override;

  // Sets the 'source' and 'origin' to indicate this is a poll request.
  void HelpPopulateGuMessage(
      sync_pb::GetUpdatesMessage* get_updates) const override;

  std::unique_ptr<ProtocolEvent> GetNetworkRequestEvent(
      base::Time timestamp,
      const sync_pb::ClientToServerMessage& request) const override;

  bool IsNotificationInfoRequired() const override;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_GET_UPDATES_DELEGATE_H_
