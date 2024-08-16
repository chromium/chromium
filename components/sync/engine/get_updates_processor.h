// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_GET_UPDATES_PROCESSOR_H_
#define COMPONENTS_SYNC_ENGINE_GET_UPDATES_PROCESSOR_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "components/sync/base/data_type.h"
#include "components/sync/engine/data_type_registry.h"
#include "components/sync/engine/syncer_error.h"

namespace sync_pb {
class GetUpdatesResponse;
class ClientToServerMessage;
}  // namespace sync_pb

namespace syncer {

class GetUpdatesDelegate;
class StatusController;
class SyncCycle;

// This class manages the set of per-type syncer objects.
//
// It owns these types and hides the details of iterating over all of them.
// Most methods allow the caller to specify a subset of types on which the
// operation is to be applied.  It is a logic error if the supplied set of types
// contains a type which was not previously registered with the manager.
class GetUpdatesProcessor {
 public:
  explicit GetUpdatesProcessor(UpdateHandlerMap* update_handler_map,
                               const GetUpdatesDelegate& delegate);

  GetUpdatesProcessor(const GetUpdatesProcessor&) = delete;
  GetUpdatesProcessor& operator=(const GetUpdatesProcessor&) = delete;

  ~GetUpdatesProcessor();

  // Downloads and processes a batch of updates for the specified types.
  //
  // Returns SYNCER_OK if the download succeeds or an appropriate error value in
  // case of failure.
  SyncerError DownloadUpdates(DataTypeSet* request_types, SyncCycle* cycle);

  // Applies any downloaded and processed updates.
  void ApplyUpdates(const DataTypeSet& gu_types,
                    StatusController* status_controller);

  // Returns true if last DownloadUpdates() outcome indicated that there are
  // more updates to download from the server, e.g. when GetUpdatesResponse has
  // non-zero `changes_remaining`.
  bool HasMoreUpdatesToDownload() const;

 private:
  // Populates a GetUpdates request message with per-type information.
  void PrepareGetUpdates(const DataTypeSet& gu_types,
                         sync_pb::ClientToServerMessage* message);

  // Sends the specified message to the server and stores the response in a
  // member of the |cycle|'s StatusController.
  SyncerError ExecuteDownloadUpdates(DataTypeSet* request_types,
                                     SyncCycle* cycle,
                                     sync_pb::ClientToServerMessage* msg);

  // Processes a GetUpdates response for each type.
  SyncerError ProcessResponse(const sync_pb::GetUpdatesResponse& gu_response,
                              const DataTypeSet& gu_types,
                              StatusController* status_controller);

  FRIEND_TEST_ALL_PREFIXES(GetUpdatesProcessorTest, BookmarkNudge);
  FRIEND_TEST_ALL_PREFIXES(GetUpdatesProcessorTest, NotifyNormalDelegate);
  FRIEND_TEST_ALL_PREFIXES(GetUpdatesProcessorTest, NotifyConfigureDelegate);
  FRIEND_TEST_ALL_PREFIXES(GetUpdatesProcessorTest,
                           NotifyPollGetUpdatesDelegate);
  FRIEND_TEST_ALL_PREFIXES(GetUpdatesProcessorTest, InitialSyncRequest);
  FRIEND_TEST_ALL_PREFIXES(GetUpdatesProcessorTest, ConfigureTest);
  FRIEND_TEST_ALL_PREFIXES(GetUpdatesProcessorTest, PollTest);
  FRIEND_TEST_ALL_PREFIXES(GetUpdatesProcessorTest, RetryTest);
  FRIEND_TEST_ALL_PREFIXES(GetUpdatesProcessorTest, NudgeWithRetryTest);
  FRIEND_TEST_ALL_PREFIXES(GetUpdatesProcessorTest, InvalidResponse);
  FRIEND_TEST_ALL_PREFIXES(GetUpdatesProcessorTest, MoreToDownloadResponse);
  FRIEND_TEST_ALL_PREFIXES(GetUpdatesProcessorTest, NormalResponseTest);

  // A map of 'update handlers', one for each enabled type.
  // This must be kept in sync with the routing info.  Our temporary solution to
  // that problem is to initialize this map in set_routing_info().
  const raw_ptr<UpdateHandlerMap> update_handler_map_;

  // Whether last GetUpdatesResponse has non-zero `changes_remaining`.
  bool has_more_updates_to_download_ = false;

  const raw_ref<const GetUpdatesDelegate> delegate_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_GET_UPDATES_PROCESSOR_H_
