// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_TELEMETRY_LOGGER_TELEMETRY_LOGGER_H_
#define CHROME_ENTERPRISE_COMPANION_TELEMETRY_LOGGER_TELEMETRY_LOGGER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/enterprise_companion/telemetry_logger/proto/log_request.pb.h"

namespace enterprise_companion::telemetry_logger {

// Provides an interface for applications to record metric events which are
// logged to a remote endpoint. Methods may be called from any sequence.
// TelemetryLogger is templated on the application-defined metric/event type.
template <typename T>
class TelemetryLogger
    : public base::RefCountedDeleteOnSequence<TelemetryLogger<T>> {
 public:
  // To be implemented by TelemetryLogger embedders.
  class Delegate {
   public:
    // Stores a value indicating when the next upload attempt may be made, as
    // indicated by the server.
    virtual bool StoreNextAllowedAttemptTime(base::Time time) = 0;
    virtual std::optional<base::Time> GetNextAllowedAttemptTime() const = 0;

    // Perform an HTTP POST request with `response_body`, invoking
    // `callback` upon completion. `http_status` and `response_body` may be
    // nullopt if the request fails. The delegate knows the target URL and is
    // responsible for attaching the logging cookie to this request and
    // persisting the value set by the server.
    virtual void DoPostRequest(
        const std::string& request_body,
        base::OnceCallback<void(std::optional<int> http_status,
                                std::optional<std::string> response_body)>
            callback) = 0;
    // Returns the unique numeric log identifier for the application.
    virtual int GetLogIdentifier() const = 0;
    // Bundle `events` into the application-defined top-level logging proto and
    // serialize it as a string.
    virtual std::string AggregateAndSerializeEvents(
        base::span<T> events) const = 0;
    // Returns the minimum cooldown time before the next transmission.
    virtual base::TimeDelta MinimumCooldownTime() const = 0;
    virtual ~Delegate() = default;
  };

  TelemetryLogger(scoped_refptr<base::SequencedTaskRunner> task_runner,
                  std::unique_ptr<Delegate> delegate)
      : base::RefCountedDeleteOnSequence<TelemetryLogger<T>>(task_runner),
        delegate_(std::move(delegate)) {
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }
  TelemetryLogger(const TelemetryLogger&) = delete;
  TelemetryLogger& operator=(const TelemetryLogger&) = delete;

  // Factory function that creates the per-process TelemetryLogger singleton.
  static scoped_refptr<TelemetryLogger> Create(
      std::unique_ptr<Delegate> delegate) {
    auto logger = base::MakeRefCounted<TelemetryLogger<T>>(
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::WithBaseSyncPrimitives()}),
        std::move(delegate));
    logger->owning_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&TelemetryLogger::SetInitialCooldownIfExists, logger));
    return logger;
  }

  // Record `event` to be transmitted. `event` is cached internally and a
  // transmission is scheduled arbitrarily in the future, respecting
  // server-instructed cooldowns. `Flush` may be used to trigger a transmission
  // immediately, if the client is not on a cool-down.
  void Log(const T& event) {
    VLOG(2) << __func__;
    owning_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&TelemetryLogger<T>::DoLog,
                                  base::WrapRefCounted(this), event));
  }

  // Trigger the transmission of cached logs if the client is not on a
  // cool-down. `callback` is answered on the calling sequence .
  // If a delayed upload is scheduled, the timer class will hold a reference
  // to this class util the timer is triggered.
  void Flush(base::OnceClosure callback) {
    owning_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &TelemetryLogger::Transmit, base::WrapRefCounted(this),
            base::BindPostTaskToCurrentDefault(std::move(callback))));
  }

  // An active cooldown timer holds a reference to this object until it is
  // triggered. Call this function to allow the immedidate release of this
  // object after all the external references are gone.
  void CancelCooldownTimer() {
    owning_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](scoped_refptr<TelemetryLogger<T>> logger) {
                         logger->cooldown_timer_.Stop();
                       },
                       base::WrapRefCounted(this)));
  }

 private:
  virtual ~TelemetryLogger() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    VLOG(1) << __func__;
  }

  base::SequencedTaskRunner* owning_task_runner() {
    return base::RefCountedDeleteOnSequence<
        TelemetryLogger<T>>::owning_task_runner();
  }

  void DoLog(const T& event) {
    VLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    events_.push_back(event);
  }

  std::string BuildRequestString(base::span<T> events) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    int64_t now_ms = base::Time::Now().InMillisecondsSinceUnixEpoch();
    proto::LogRequest request;
    request.set_request_time_ms(now_ms);
    request.mutable_client_info()->set_client_type(
        telemetry_logger::proto::
            ClientInfo_ClientType_CHROME_ENTERPRISE_COMPANION);
    request.set_log_source(delegate_->GetLogIdentifier());
    telemetry_logger::proto::LogEvent* log_event = request.add_log_event();
    log_event->set_event_time_ms(now_ms);
    log_event->set_source_extension(
        delegate_->AggregateAndSerializeEvents(events));
    return request.SerializeAsString();
  }

  void Transmit(base::OnceClosure callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (is_transmitting_) {
      VLOG(2) << "Transmit skipped when there's an active one.";
      std::move(callback).Run();
      return;
    }
    if (cooldown_timer_.IsRunning()) {
      VLOG(2) << "Transmit skipped in cool down period.";
      std::move(callback).Run();
      return;
    }

    if (events_.empty() && upload_queue_.empty()) {
      VLOG(2) << "No events to transmit.";
      std::move(callback).Run();
      return;
    }

    // Move all events to the upload queue. The upload queue is kept until
    // the upload transaction is completed deterministically.
    if (!events_.empty()) {
      upload_queue_.insert(upload_queue_.end(),
                           std::make_move_iterator(events_.begin()),
                           std::make_move_iterator(events_.end()));
      events_.clear();
    }

    VLOG(2) << "Transmitting " << upload_queue_.size() << " events at "
            << base::Time::Now();
    is_transmitting_ = true;
    delegate_->DoPostRequest(
        BuildRequestString(upload_queue_),
        base::BindOnce(&TelemetryLogger::OnResponseReceived,
                       base::WrapRefCounted(this))
            .Then(std::move(callback)));
  }

  bool ShouldDeleteUploadQueue(std::optional<int> http_status) const {
    if (!http_status) {
      return false;
    }

    // Delete the upload queue for the 2xx and 4xx family of responses.
    return (*http_status >= 200 && *http_status < 300) ||
           (*http_status >= 400 && *http_status < 500);
  }

  void OnResponseReceived(std::optional<int> http_status,
                          std::optional<std::string> response_body) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    VLOG(1) << __func__ << ": status=" << http_status.value_or(0);
    is_transmitting_ = false;
    if (ShouldDeleteUploadQueue(http_status)) {
      VLOG(2) << "Clearing the upload queue.";
      upload_queue_.clear();
    }

    telemetry_logger::proto::LogResponse response;
    if (!response_body || !response.ParseFromString(*response_body)) {
      LOG(ERROR) << "Failed to parse log response proto, response body: "
                 << response_body.value_or("");
      SetCooldown(delegate_->MinimumCooldownTime());
      return;
    }

    base::TimeDelta cooldown_time =
        std::max(base::Milliseconds(response.next_request_wait_millis()),
                 delegate_->MinimumCooldownTime());
    VLOG(1) << "Cooldown time received from server: "
            << response.next_request_wait_millis() << " ms";
    delegate_->StoreNextAllowedAttemptTime(base::Time::Now() + cooldown_time);
    SetCooldown(cooldown_time);
  }

  void SetCooldown(base::TimeDelta cooldown_time) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(!cooldown_timer_.IsRunning());
    if (cooldown_time.is_negative()) {
      return;
    }

    VLOG(2) << "Set cool down time: " << cooldown_time.InMilliseconds()
            << "ms, " << base::Time::Now() + cooldown_time;
    cooldown_timer_.Start(
        FROM_HERE, cooldown_time,
        base::BindOnce(&TelemetryLogger::Transmit, base::WrapRefCounted(this),
                       base::DoNothing()));
  }

  void SetInitialCooldownIfExists() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(!cooldown_timer_.IsRunning());

    std::optional<base::Time> next_allowed_attempt_time =
        delegate_->GetNextAllowedAttemptTime();
    if (!next_allowed_attempt_time) {
      return;
    }
    VLOG(2) << __func__
            << ": next allowed attempt time: " << *next_allowed_attempt_time;
    SetCooldown(*next_allowed_attempt_time - base::Time::Now());
  }

  friend class base::RefCountedDeleteOnSequence<TelemetryLogger<T>>;
  friend class base::DeleteHelper<TelemetryLogger<T>>;

  bool is_transmitting_ = false;
  std::unique_ptr<Delegate> delegate_;
  std::vector<T> events_;
  std::vector<T> upload_queue_;
  base::OneShotTimer cooldown_timer_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace enterprise_companion::telemetry_logger

#endif  // CHROME_ENTERPRISE_COMPANION_TELEMETRY_LOGGER_TELEMETRY_LOGGER_H_
