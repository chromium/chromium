// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_GET_UPDATES_DELEGATE_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_GET_UPDATES_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "components/sync/engine/events/protocol_event.h"
#include "components/sync/engine_impl/cycle/nudge_tracker.h"
#include "components/sync/engine_impl/cycle/status_controller.h"
#include "components/sync/engine_impl/model_type_registry.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

class GetUpdatesProcessor;

// Interface for GetUpdates functionality that depends on the requested
// GetUpdate type (normal, configuration, poll).  The GetUpdatesProcessor is
// given an appropriate GetUpdatesDelegate to handle type specific functionality
// on construction.
class GetUpdatesDelegate {
 public:
  GetUpdatesDelegate();
  virtual ~GetUpdatesDelegate() = 0;

  // Populates GetUpdate message fields that depend on GetUpdates request type.
  virtual void HelpPopulateGuMessage(
      sync_pb::GetUpdatesMessage* get_updates) const = 0;

  // Applies pending updates to non-control types.
  virtual void ApplyUpdates(const ModelTypeSet& gu_types,
                            StatusController* status,
                            UpdateHandlerMap* update_handler_map) const = 0;

  virtual std::unique_ptr<ProtocolEvent> GetNetworkRequestEvent(
      base::Time timestamp,
      const sync_pb::ClientToServerMessage& request) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(GetUpdatesDelegate);
};

// Functionality specific to the normal GetUpdate request.
class NormalGetUpdatesDelegate : public GetUpdatesDelegate {
 public:
  explicit NormalGetUpdatesDelegate(const NudgeTracker& nudge_tracker);
  ~NormalGetUpdatesDelegate() override;

  // Uses the member NudgeTracker to populate some fields of this GU message.
  void HelpPopulateGuMessage(
      sync_pb::GetUpdatesMessage* get_updates) const override;

  // Applies pending updates on the appropriate data type threads.
  void ApplyUpdates(const ModelTypeSet& gu_types,
                    StatusController* status,
                    UpdateHandlerMap* update_handler_map) const override;

  std::unique_ptr<ProtocolEvent> GetNetworkRequestEvent(
      base::Time timestamp,
      const sync_pb::ClientToServerMessage& request) const override;

 private:
  const NudgeTracker& nudge_tracker_;

  DISALLOW_COPY_AND_ASSIGN(NormalGetUpdatesDelegate);
};

// Functionality specific to the configure GetUpdate request.
class ConfigureGetUpdatesDelegate : public GetUpdatesDelegate {
 public:
  explicit ConfigureGetUpdatesDelegate(
      sync_pb::SyncEnums::GetUpdatesOrigin origin);
  ~ConfigureGetUpdatesDelegate() override;

  // Sets the 'source' and 'origin' fields for this request.
  void HelpPopulateGuMessage(
      sync_pb::GetUpdatesMessage* get_updates) const override;

  // Applies updates passively (i.e. on the sync thread).
  //
  // This is safe only if the ChangeProcessor is not listening to changes at
  // this time.
  void ApplyUpdates(const ModelTypeSet& gu_types,
                    StatusController* status,
                    UpdateHandlerMap* update_handler_map) const override;

  std::unique_ptr<ProtocolEvent> GetNetworkRequestEvent(
      base::Time timestamp,
      const sync_pb::ClientToServerMessage& request) const override;

 private:
  const sync_pb::SyncEnums::GetUpdatesOrigin origin_;

  DISALLOW_COPY_AND_ASSIGN(ConfigureGetUpdatesDelegate);
};

// Functionality specific to the poll GetUpdate request.
class PollGetUpdatesDelegate : public GetUpdatesDelegate {
 public:
  PollGetUpdatesDelegate();
  ~PollGetUpdatesDelegate() override;

  // Sets the 'source' and 'origin' to indicate this is a poll request.
  void HelpPopulateGuMessage(
      sync_pb::GetUpdatesMessage* get_updates) const override;

  // Applies updates on the appropriate data type thread.
  void ApplyUpdates(const ModelTypeSet& gu_types,
                    StatusController* status,
                    UpdateHandlerMap* update_handler_map) const override;

  std::unique_ptr<ProtocolEvent> GetNetworkRequestEvent(
      base::Time timestamp,
      const sync_pb::ClientToServerMessage& request) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PollGetUpdatesDelegate);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_GET_UPDATES_DELEGATE_H_
