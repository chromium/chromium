// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/util/disconnectable_client.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/containers/fixed_flat_map.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/reporting/util/status.h"

namespace reporting {

DisconnectableClient::DisconnectableClient(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(task_runner) {}

DisconnectableClient::~DisconnectableClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetAvailability(/*is_available=*/false);
}

void DisconnectableClient::MaybeMakeCall(std::unique_ptr<Delegate> delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Bail out, if missive daemon is not available over dBus.
  if (!is_available_) {
    delegate->Respond(
        Status(reporting::error::UNAVAILABLE, "Service is unavailable"));
    return;
  }
  // Add the delegate to the map.
  const auto id = base::RandUint64();
  auto res = outstanding_delegates_.emplace(id, std::move(delegate));
  DCHECK(res.second) << "Duplicate call id " << id;
  // Make a call, resume on CallResponded, when response is received.
  res.first->second->DoCall(base::BindOnce(&DisconnectableClient::CallResponded,
                                           weak_ptr_factory_.GetWeakPtr(), id));
}

void DisconnectableClient::CallResponded(uint64_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = outstanding_delegates_.find(id);
  if (it == outstanding_delegates_.end()) {
    // Callback has already been removed, no action needed.
    return;
  }
  // Respond through the |delegate|.
  auto delegate = std::move(it->second);
  outstanding_delegates_.erase(it);
  // Respond.
  delegate->Respond(Status::StatusOK());
}

void DisconnectableClient::SetAvailability(bool is_available) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_available_ = is_available;
  LOG(WARNING) << "Service became " << (is_available_ ? "" : "un")
               << "available";
  if (!is_available_) {
    // Cancel all pending calls.
    for (auto& p : outstanding_delegates_) {
      p.second->Respond(
          Status(reporting::error::UNAVAILABLE, "Service is unavailable"));
      // Release the delegate sooner, don't wait until clear().
      p.second.reset();
    }
    outstanding_delegates_.clear();
  }
}

}  // namespace reporting
