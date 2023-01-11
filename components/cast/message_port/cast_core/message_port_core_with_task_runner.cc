// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast/message_port/cast_core/message_port_core_with_task_runner.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"

namespace cast_api_bindings {

namespace {
static uint32_t GenerateChannelId() {
  // Should theoretically start at a random number to lower collision chance if
  // ports are created in multiple places, but in practice this does not happen
  static std::atomic<uint32_t> channel_id = {0x8000000};
  return ++channel_id;
}
}  // namespace

std::pair<MessagePortCoreWithTaskRunner, MessagePortCoreWithTaskRunner>
MessagePortCoreWithTaskRunner::CreatePair() {
  auto channel_id = GenerateChannelId();
  auto pair = std::make_pair(MessagePortCoreWithTaskRunner(channel_id),
                             MessagePortCoreWithTaskRunner(channel_id));
  pair.first.SetPeer(&pair.second);
  pair.second.SetPeer(&pair.first);
  return pair;
}

MessagePortCoreWithTaskRunner::MessagePortCoreWithTaskRunner(
    uint32_t channel_id)
    : MessagePortCore(channel_id) {}

MessagePortCoreWithTaskRunner::MessagePortCoreWithTaskRunner(
    MessagePortCoreWithTaskRunner&& other)
    : MessagePortCore(std::move(other)) {
  task_runner_ = std::exchange(other.task_runner_, nullptr);
}

MessagePortCoreWithTaskRunner::~MessagePortCoreWithTaskRunner() = default;

MessagePortCoreWithTaskRunner& MessagePortCoreWithTaskRunner::operator=(
    MessagePortCoreWithTaskRunner&& other) {
  task_runner_ = std::exchange(other.task_runner_, nullptr);
  Assign(std::move(other));

  return *this;
}

void MessagePortCoreWithTaskRunner::SetTaskRunner() {
  task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
}

void MessagePortCoreWithTaskRunner::AcceptOnSequence(Message message) {
  DCHECK(task_runner_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MessagePortCoreWithTaskRunner::AcceptInternal,
                     weak_factory_.GetWeakPtr(), std::move(message)));
}

void MessagePortCoreWithTaskRunner::AcceptResultOnSequence(bool result) {
  DCHECK(task_runner_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MessagePortCoreWithTaskRunner::AcceptResultInternal,
                     weak_factory_.GetWeakPtr(), result));
}

void MessagePortCoreWithTaskRunner::CheckPeerStartedOnSequence() {
  DCHECK(task_runner_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MessagePortCoreWithTaskRunner::CheckPeerStartedInternal,
                     weak_factory_.GetWeakPtr()));
}

void MessagePortCoreWithTaskRunner::StartOnSequence() {
  DCHECK(task_runner_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&MessagePortCoreWithTaskRunner::Start,
                                        weak_factory_.GetWeakPtr()));
}

void MessagePortCoreWithTaskRunner::PostMessageOnSequence(Message message) {
  DCHECK(task_runner_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MessagePortCoreWithTaskRunner::PostMessageInternal,
                     weak_factory_.GetWeakPtr(), std::move(message)));
}

void MessagePortCoreWithTaskRunner::OnPipeErrorOnSequence() {
  DCHECK(task_runner_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MessagePortCoreWithTaskRunner::OnPipeErrorInternal,
                     weak_factory_.GetWeakPtr()));
}

bool MessagePortCoreWithTaskRunner::HasTaskRunner() const {
  return !!task_runner_;
}

}  // namespace cast_api_bindings
