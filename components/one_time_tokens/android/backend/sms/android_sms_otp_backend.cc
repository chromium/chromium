// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/android/backend/sms/android_sms_otp_backend.h"

#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/one_time_tokens/android/backend/sms/sms_otp_retrieval_api_error_codes.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"

namespace one_time_tokens {

AndroidSmsOtpBackend::AndroidSmsOtpBackend()
    : receiver_bridge_(AndroidSmsOtpFetchReceiverBridge::Create()),
      dispatcher_bridge_(AndroidSmsOtpFetchDispatcherBridge::Create()),
      background_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::TaskPriority::USER_VISIBLE})) {
  InitBridges();
}

AndroidSmsOtpBackend::AndroidSmsOtpBackend(
    base::PassKey<class AndroidSmsOtpBackendTest>,
    std::unique_ptr<AndroidSmsOtpFetchReceiverBridgeInterface> receiver_bridge,
    std::unique_ptr<AndroidSmsOtpFetchDispatcherBridgeInterface>
        dispatcher_bridge,
    scoped_refptr<base::SingleThreadTaskRunner> background_task_runner)
    : receiver_bridge_(std::move(receiver_bridge)),
      dispatcher_bridge_(std::move(dispatcher_bridge)),
      background_task_runner_(background_task_runner) {
  InitBridges();
}

AndroidSmsOtpBackend::~AndroidSmsOtpBackend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  // Delete dispatcher bridge on the background thread where it lives.
  background_task_runner_->DeleteSoon(FROM_HERE, std::move(dispatcher_bridge_));
}

void AndroidSmsOtpBackend::RetrieveSmsOtp(
    base::OnceCallback<void(const OtpFetchReply&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  if (!initialization_result_.has_value()) {
    // The downstream backend initialization is in progress, postpone the call.
    has_pending_fetch_request_ = true;
    pending_callbacks_.push(std::move(callback));
    return;
  }

  // Return early if the downstream backend did not initialize successfully.
  if (!initialization_result_.value()) {
    std::move(callback).Run(
        OtpFetchReply{/*otp_value=*/std::nullopt, /*request_complete=*/false});
    return;
  }

  // Callbacks are simply stored in a queue, because with Android SMS OTPs API
  // is not able to differentiate between senders and websites origins, so if
  // the API is invoked a few times during a short period of time, the replies
  // are the same and it makes no sense to add a more sophisticated mechanism to
  // make sure that the callbacks and requests match.
  pending_callbacks_.push(std::move(callback));

  StartDownstreamBackendRequest();
}

void AndroidSmsOtpBackend::OnOtpValueRetrieved(std::string value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  if (!pending_callbacks_.empty()) {
    std::move(pending_callbacks_.front())
        .Run(OtpFetchReply{OneTimeToken(OneTimeTokenType::kSmsOtp,
                                        std::move(value), base::Time::Now()),
                           /*request_complete_=*/true});
    pending_callbacks_.pop();
  }
}

void AndroidSmsOtpBackend::OnOtpValueRetrievalError(
    SmsOtpRetrievalApiErrorCode error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  // TODO(crbug.com/415272524): Record metrics on the API error codes.

  if (!pending_callbacks_.empty()) {
    // kTimeout means that nothing prevented the request from execution, but the
    // SMS with the OTP value was not received within some time. All other
    // errors mean that it was not possible to execute the request.
    bool request_complete =
        (error_code == SmsOtpRetrievalApiErrorCode::kTimeout);
    std::move(pending_callbacks_.front())
        .Run(OtpFetchReply{/*otp_value=*/std::nullopt, request_complete});
    pending_callbacks_.pop();
  }
}

std::optional<bool> AndroidSmsOtpBackend::GetInitializationResultForTesting()
    const {
  return initialization_result_;
}

void AndroidSmsOtpBackend::InitBridges() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  receiver_bridge_->SetConsumer(weak_ptr_factory_.GetWeakPtr());
  // The dispatcher bridge is deleted manually in this class' destructor on the
  // sequence where all operations of this class are executed. It's safe to use
  // `base::Unretained(dispatcher_bridge_)` for binding here.
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&AndroidSmsOtpFetchDispatcherBridgeInterface::Init,
                     base::Unretained(dispatcher_bridge_.get()),
                     receiver_bridge_->GetJavaBridge()),
      base::BindOnce(&AndroidSmsOtpBackend::OnBridgesInitComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AndroidSmsOtpBackend::OnBridgesInitComplete(bool init_success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  initialization_result_ = init_success;

  if (init_success && has_pending_fetch_request_) {
    // A request was received while initialization was in progress. Now that
    // initialization is complete, trigger the request.
    has_pending_fetch_request_ = false;
    StartDownstreamBackendRequest();
  }
}

void AndroidSmsOtpBackend::StartDownstreamBackendRequest() {
  // The dispatcher bridge is deleted manually in this class' destructor on the
  // sequence where all operations of this class are executed. It's safe to use
  // `base::Unretained(dispatcher_bridge_)` for binding here.
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AndroidSmsOtpFetchDispatcherBridgeInterface::RetrieveSmsOtp,
          base::Unretained(dispatcher_bridge_.get())));
}

}  // namespace one_time_tokens
