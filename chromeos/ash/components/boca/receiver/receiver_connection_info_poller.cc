// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/receiver/receiver_connection_info_poller.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/boca_request.h"
#include "chromeos/ash/components/boca/proto/receiver.pb.h"
#include "chromeos/ash/components/boca/receiver/get_receiver_connection_info_request.h"
#include "chromeos/ash/components/boca/retriable_request_sender.h"
#include "google_apis/common/request_sender.h"

namespace ash::boca_receiver {
namespace {

base::TimeDelta GetPollingInterval() {
  constexpr base::TimeDelta kDefaultPollingInterval = base::Seconds(10);
  return ash::features::IsBocaReceiverCustomPollingEnabled()
             ? ash::features::kBocaReceiverCustomPollingInterval.Get()
             : kDefaultPollingInterval;
}

int GetMaxConsecutiveFailures() {
  constexpr int kDefaultMaxConsecutiveFailures = 3;
  return ash::features::IsBocaReceiverCustomPollingEnabled()
             ? ash::features::kBocaReceiverCustomPollingMaxFailuresCount.Get()
             : kDefaultMaxConsecutiveFailures;
}

}  // namespace

ReceiverConnectionInfoPoller::ReceiverConnectionInfoPoller() = default;
ReceiverConnectionInfoPoller::~ReceiverConnectionInfoPoller() = default;

void ReceiverConnectionInfoPoller::Start(
    const std::string& receiver_id,
    const std::string& connection_id,
    std::unique_ptr<google_apis::RequestSender> request_sender,
    OnStopCallback on_stop_callback) {
  constexpr int kMaxRetriesPerRequest = 1;
  Stop();
  retriable_sender_ = std::make_unique<
      ash::boca::RetriableRequestSender<::boca::KioskReceiverConnection>>(
      std::move(request_sender), kMaxRetriesPerRequest);

  polling_timer_.Start(
      FROM_HERE, GetPollingInterval(),
      base::BindOnce(&ReceiverConnectionInfoPoller::PollConnectionInfo,
                     base::Unretained(this), receiver_id, connection_id,
                     std::move(on_stop_callback)));
}

void ReceiverConnectionInfoPoller::Stop() {
  polling_timer_.Stop();
  retriable_sender_.reset();
  consecutive_failure_count_ = 0;
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void ReceiverConnectionInfoPoller::PollConnectionInfo(
    const std::string& receiver_id,
    const std::string& connection_id,
    OnStopCallback on_stop_callback) {
  auto create_delegate_callback = base::BindRepeating(
      [](const std::string& receiver_id, const std::string& connection_id,
         base::OnceCallback<void(
             std::optional<::boca::KioskReceiverConnection>)> callback)
          -> std::unique_ptr<ash::boca::BocaRequest::Delegate> {
        return std::make_unique<
            boca_receiver::GetReceiverConnectionInfoRequest>(
            receiver_id, connection_id, std::move(callback));
      },
      receiver_id, connection_id);

  retriable_sender_->SendRequest(
      create_delegate_callback,
      base::BindPostTaskToCurrentDefault(
          base::BindOnce(&ReceiverConnectionInfoPoller::OnConnectionInfoPolled,
                         weak_ptr_factory_.GetWeakPtr(), receiver_id,
                         connection_id, std::move(on_stop_callback))));
}

void ReceiverConnectionInfoPoller::OnConnectionInfoPolled(
    const std::string& receiver_id,
    const std::string& connection_id,
    OnStopCallback on_stop_callback,
    std::optional<::boca::KioskReceiverConnection> response) {
  consecutive_failure_count_ =
      response.has_value() ? 0 : consecutive_failure_count_ + 1;
  bool server_unreachable =
      consecutive_failure_count_ >= GetMaxConsecutiveFailures();
  if (server_unreachable ||
      (response.has_value() &&
       response->receiver_connection_state() ==
           ::boca::ReceiverConnectionState::STOP_REQUESTED)) {
    std::move(on_stop_callback).Run(server_unreachable);
    Stop();
    return;
  }
  polling_timer_.Start(
      FROM_HERE, GetPollingInterval(),
      base::BindOnce(&ReceiverConnectionInfoPoller::PollConnectionInfo,
                     base::Unretained(this), receiver_id, connection_id,
                     std::move(on_stop_callback)));
}

}  // namespace ash::boca_receiver
