// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_RETRIABLE_REQUEST_SENDER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_RETRIABLE_REQUEST_SENDER_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/boca_request.h"
#include "google_apis/common/request_sender.h"
#include "net/base/backoff_entry.h"

namespace ash::boca {

template <class T>
class RetriableRequestSender {
 public:
  using ResponseCallback = base::OnceCallback<void(std::optional<T>)>;
  using CreateDelegateCallback =
      base::RepeatingCallback<std::unique_ptr<BocaRequest::Delegate>(
          ResponseCallback)>;

  RetriableRequestSender(
      std::unique_ptr<google_apis::RequestSender> request_sender,
      int max_retries)
      : request_sender_(std::move(request_sender)),
        max_retries_(max_retries),
        backoff_policy_{.num_errors_to_ignore = 0,
                        .initial_delay_ms = 250,
                        .multiply_factor = 2,
                        .jitter_factor = 0.2,
                        .maximum_backoff_ms =
                            -1,  // Rely on maximum number of retries instead.
                        .entry_lifetime_ms = -1,
                        .always_use_initial_delay = true} {}

  RetriableRequestSender(const RetriableRequestSender&) = delete;
  RetriableRequestSender& operator=(const RetriableRequestSender&) = delete;

  ~RetriableRequestSender() = default;

  void SendRequest(CreateDelegateCallback create_delegate_callback,
                   ResponseCallback response_callback) {
    auto backoff_entry = std::make_unique<net::BackoffEntry>(&backoff_policy_);
    SendRequestInternal(std::move(create_delegate_callback),
                        std::move(response_callback), std::move(backoff_entry));
  }

 private:
  void SendRequestInternal(CreateDelegateCallback create_delegate_callback,
                           ResponseCallback response_callback,
                           std::unique_ptr<net::BackoffEntry> backoff_entry) {
    auto on_response =
        base::BindOnce(&RetriableRequestSender::OnResponse,
                       weak_ptr_factory_.GetWeakPtr(), create_delegate_callback,
                       std::move(response_callback), std::move(backoff_entry));
    auto delegate = create_delegate_callback.Run(std::move(on_response));
    auto request = std::make_unique<BocaRequest>(request_sender_.get(),
                                                 std::move(delegate));
    request_sender_->StartRequestWithAuthRetry(std::move(request));
  }

  void OnResponse(CreateDelegateCallback create_delegate_callback,
                  ResponseCallback response_callback,
                  std::unique_ptr<net::BackoffEntry> backoff_entry,
                  std::optional<T> response) {
    if (response || backoff_entry->failure_count() >= max_retries_) {
      std::move(response_callback).Run(response);
      return;
    }
    backoff_entry->InformOfRequest(false);
    const base::TimeDelta delay = backoff_entry->GetTimeUntilRelease();
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&RetriableRequestSender::SendRequestInternal,
                       weak_ptr_factory_.GetWeakPtr(), create_delegate_callback,
                       std::move(response_callback), std::move(backoff_entry)),
        delay);
  }

  const std::unique_ptr<google_apis::RequestSender> request_sender_;
  const int max_retries_;
  const net::BackoffEntry::Policy backoff_policy_;
  base::WeakPtrFactory<RetriableRequestSender<T>> weak_ptr_factory_{this};
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_RETRIABLE_REQUEST_SENDER_H_
