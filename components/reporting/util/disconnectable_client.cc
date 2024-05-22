// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/util/disconnectable_client.h"

#include <memory>
#include <utility>

#include "base/containers/fixed_flat_map.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/reporting/util/reporting_errors.h"
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
    delegate->Respond(Status(error::UNAVAILABLE,
                             disconnectable_client::kErrorServiceUnavailable));
    base::UmaHistogramEnumeration(
        reporting::kUmaUnavailableErrorReason,
        UnavailableErrorReason::CLIENT_NOT_CONNECTED_TO_MISSIVE,
        UnavailableErrorReason::MAX_VALUE);
    return;
  }
  // Add the delegate to the map.
  const auto id = ++last_id_;
  auto res = outstanding_delegates_.emplace(id, std::move(delegate));
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
  // Remove delegate from |outstanding_delegates_|.
  const auto delegate = std::move(it->second);
  outstanding_delegates_.erase(it);
  // Respond through the |delegate|.
  delegate->Respond(Status::StatusOK());
}

void DisconnectableClient::SetAvailability(bool is_available) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_available_ = is_available;
  LOG(WARNING) << "Service became " << (is_available_ ? "" : "un")
               << "available";
  if (!is_available_) {
    // Cancel all pending calls.
    while (!outstanding_delegates_.empty()) {
      // Remove the first delegate from |outstanding_delegates_|.
      const auto delegate = std::move(outstanding_delegates_.begin()->second);
      outstanding_delegates_.erase(outstanding_delegates_.begin());
      // Respond through the |delegate|.
      delegate->Respond(Status(
          error::UNAVAILABLE, disconnectable_client::kErrorServiceUnavailable));
      base::UmaHistogramEnumeration(
          reporting::kUmaUnavailableErrorReason,
          UnavailableErrorReason::CLIENT_NOT_CONNECTED_TO_MISSIVE,
          UnavailableErrorReason::MAX_VALUE);
    }
  }
}

}  // namespace reporting
