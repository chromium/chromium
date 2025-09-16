// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_ANDROID_BACKEND_SMS_ANDROID_SMS_OTP_BACKEND_H_
#define COMPONENTS_ONE_TIME_TOKENS_ANDROID_BACKEND_SMS_ANDROID_SMS_OTP_BACKEND_H_

#include <optional>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/pass_key.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/one_time_tokens/android/backend/sms/android_sms_otp_fetch_dispatcher_bridge.h"
#include "components/one_time_tokens/android/backend/sms/android_sms_otp_fetch_receiver_bridge.h"
#include "components/one_time_tokens/core/browser/sms_otp_backend.h"

namespace one_time_tokens {

// This class processes SMS OTP requests and propagates back the replies with
// OTP values, 1 per profile.
class AndroidSmsOtpBackend
    : public KeyedService,
      public SmsOtpBackend,
      public AndroidSmsOtpFetchReceiverBridgeInterface::Consumer {
 public:
  AndroidSmsOtpBackend();
  AndroidSmsOtpBackend(
      base::PassKey<class AndroidSmsOtpBackendTest>,
      std::unique_ptr<AndroidSmsOtpFetchReceiverBridgeInterface>
          receiver_bridge,
      std::unique_ptr<AndroidSmsOtpFetchDispatcherBridgeInterface>
          dispatcher_bridge,
      scoped_refptr<base::SingleThreadTaskRunner> background_task_runner);

  AndroidSmsOtpBackend(const AndroidSmsOtpBackend&) = delete;
  AndroidSmsOtpBackend& operator=(const AndroidSmsOtpBackend&) = delete;
  ~AndroidSmsOtpBackend() override;

  // SmsOtpBackend:
  void RetrieveSmsOtp(
      base::OnceCallback<void(const OtpFetchReply&)> callback) override;

  // AndroidSmsOtpFetchReceiverBridge::Consumer
  void OnOtpValueRetrieved(std::string value) override;
  void OnOtpValueRetrievalError(
      SmsOtpRetrievalApiErrorCode error_code) override;

  // Getter for tests to check the initialization state.
  std::optional<bool> GetInitializationResultForTesting() const;

 private:
  // Initializes bridges, which triggers initialization of the downstream
  // implementation.
  void InitBridges();

  // Invoked when the downstream implementation in initialized.
  void OnBridgesInitComplete(bool init_success);

  // Triggers the OTP value retrieval from the backend, if it's possible.
  void StartDownstreamBackendRequest();

  // True if the downstream initialization was successful.
  std::optional<bool> initialization_result_ = false;

  // True if OTP fetching request is received before it can be triggered (before
  // the downstream initialization is complete).
  bool has_pending_fetch_request_ = false;

  // A bridge to communicate Java OTP fetcher replies back to the native code.
  std::unique_ptr<AndroidSmsOtpFetchReceiverBridgeInterface> receiver_bridge_;

  // A bridge to send OTP fetch requests to Java.
  std::unique_ptr<AndroidSmsOtpFetchDispatcherBridgeInterface>
      dispatcher_bridge_;

  // Background thread pool task runner to execute all backend operations.
  // Limited to a single thread as JNIEnv is only suitable for use on a single
  // thread.
  scoped_refptr<base::SingleThreadTaskRunner> background_task_runner_;

  // Callbacks that needs to be invoked after the OTP retrieval is complete.
  base::queue<base::OnceCallback<void(const OtpFetchReply&)>>
      pending_callbacks_;

  // All methods should be called on the main thread.
  SEQUENCE_CHECKER(main_sequence_checker_);

  base::WeakPtrFactory<AndroidSmsOtpBackend> weak_ptr_factory_{this};
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_ANDROID_BACKEND_SMS_ANDROID_SMS_OTP_BACKEND_H_
