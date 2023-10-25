// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/invalidation.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "components/invalidation/public/ack_handler.h"
#include "components/invalidation/public/invalidation_util.h"

namespace invalidation {

Invalidation::Invalidation(const Topic& topic,
                           int64_t version,
                           const std::string& payload)
    : topic_(topic), version_(version), payload_(payload) {}

Invalidation::Invalidation(const Invalidation& other) = default;

Invalidation& Invalidation::operator=(const Invalidation& other) = default;

Invalidation::~Invalidation() = default;

Topic Invalidation::topic() const {
  return topic_;
}

int64_t Invalidation::version() const {
  return version_;
}

const std::string& Invalidation::payload() const {
  return payload_;
}

const AckHandle& Invalidation::ack_handle() const {
  return ack_handle_;
}

void Invalidation::SetAckHandler(
    base::WeakPtr<AckHandler> handler,
    scoped_refptr<base::SequencedTaskRunner> handler_task_runner) {
  ack_handler_ = handler;
  ack_handler_task_runner_ = handler_task_runner;
}

void Invalidation::Acknowledge() const {
  if (ack_handler_task_runner_) {
    ack_handler_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&AckHandler::Acknowledge, ack_handler_,
                                  topic(), ack_handle_));
  }
}

bool Invalidation::operator==(const Invalidation& other) const {
  return topic_ == other.topic_ &&
         version_ == other.version_ && payload_ == other.payload_;
}

}  // namespace invalidation
