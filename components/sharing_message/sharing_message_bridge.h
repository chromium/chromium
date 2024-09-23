// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_SHARING_MESSAGE_BRIDGE_H_
#define COMPONENTS_SHARING_MESSAGE_SHARING_MESSAGE_BRIDGE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/protocol/sharing_message_specifics.pb.h"

namespace syncer {
class DataTypeControllerDelegate;
}  // namespace syncer

// Class to provide an interface to send sharing messages using Sync.
class SharingMessageBridge : public KeyedService {
 public:
  using CommitFinishedCallback =
      base::OnceCallback<void(const sync_pb::SharingMessageCommitError&)>;

  // Sends Sharing Message to Sync server. |on_commit_callback| will be called
  // when commit attempt finishes (either successfully or unsuccessfully).
  // TODO(crbug.com/40111980): take each parameter separately and construct
  // specifics inside. Currently this method updates given |specifics| and
  // fills in |message_id| field.
  virtual void SendSharingMessage(
      std::unique_ptr<sync_pb::SharingMessageSpecifics> specifics,
      CommitFinishedCallback on_commit_callback) = 0;

  // Returns the delegate for the controller, i.e. sync integration point.
  virtual base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetControllerDelegate() = 0;
};

#endif  // COMPONENTS_SHARING_MESSAGE_SHARING_MESSAGE_BRIDGE_H_
