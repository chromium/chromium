// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_PASSWORD_CHANGE_SAVE_PASSWORD_LEAK_DETECTION_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_PASSWORD_CHANGE_SAVE_PASSWORD_LEAK_DETECTION_DELEGATE_H_

#include <string>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/password_manager/core/browser/leak_detection_delegate.h"

namespace password_manager {
class LeakDetectionCheck;
class LeakDetectionCheckFactory;
struct PasswordForm;
class PasswordManagerClient;
}  // namespace password_manager

namespace autofill_assistant {

enum class LeakDetectionStatusCode {
  // The check was successful.
  SUCCESS = 0,
  // The check took too long and timed out.
  TIMEOUT = 1,
  // The check was aborted because another one was started.
  ABORTED = 2,
  // The check could not be started because the user is in incognito mode.
  INCOGNITO_MODE = 2,
  // The check could not be started because a username was missing.
  NO_USERNAME = 3,
  // The check could not be started because leak checks are disabled globally.
  DISABLED = 4,
  // The check could not be started because APC leak checks are disabled.
  DISABLED_FOR_APC = 5,
  // The check could not be started because of other reasons (e.g. factory)
  // could not be created
  OTHER = 6,
  // The leak check was started, but returned an error
  EXECUTION_ERROR = 7,
  MAX_VALUE = EXECUTION_ERROR,
};

struct LeakDetectionStatus {
  explicit LeakDetectionStatus(
      LeakDetectionStatusCode result_code = LeakDetectionStatusCode::SUCCESS)
      : result(result_code) {}

  explicit LeakDetectionStatus(password_manager::LeakDetectionError error)
      : result(LeakDetectionStatusCode::EXECUTION_ERROR),
        execution_error(error) {}

  bool IsSuccess() const { return result == LeakDetectionStatusCode::SUCCESS; }
  bool operator==(const LeakDetectionStatus& other) const {
    return (result == other.result && execution_error == other.execution_error);
  }

  LeakDetectionStatusCode result;
  absl::optional<password_manager::LeakDetectionError> execution_error;
};

class SavePasswordLeakDetectionDelegate
    : public password_manager::LeakDetectionDelegateInterface {
 public:
  explicit SavePasswordLeakDetectionDelegate(
      password_manager::PasswordManagerClient* client);
  ~SavePasswordLeakDetectionDelegate() override;

  // Not copyable or movable.
  SavePasswordLeakDetectionDelegate(const SavePasswordLeakDetectionDelegate&) =
      delete;
  SavePasswordLeakDetectionDelegate& operator=(
      const SavePasswordLeakDetectionDelegate&) = delete;
  SavePasswordLeakDetectionDelegate(SavePasswordLeakDetectionDelegate&&) =
      delete;
  SavePasswordLeakDetectionDelegate& operator=(
      SavePasswordLeakDetectionDelegate&&) = delete;

  using Callback = base::OnceCallback<void(LeakDetectionStatus, bool)>;

  void StartLeakCheck(const password_manager::PasswordForm& credential,
                      Callback callback,
                      base::TimeDelta timeout);

#if defined(UNIT_TEST)
  void set_leak_factory(
      std::unique_ptr<password_manager::LeakDetectionCheckFactory> factory) {
    leak_factory_ = std::move(factory);
  }

  password_manager::LeakDetectionCheck* leak_check() const {
    return leak_check_.get();
  }
#endif  // defined(UNIT_TEST)

 private:
  // LeakDetectionDelegateInterface:
  void OnLeakDetectionDone(bool is_leaked,
                           GURL,
                           std::u16string,
                           std::u16string) override;

  void OnError(password_manager::LeakDetectionError error) override;

  void OnLeakDetectionTimeout();

  // Must outlive the SavePasswordLeakDetectionDelegate.
  raw_ptr<password_manager::PasswordManagerClient, DanglingUntriaged> client_;

  // The factory that creates objects for performing a leak check up.
  std::unique_ptr<password_manager::LeakDetectionCheckFactory> leak_factory_;

  // Current leak check-up being performed in the background.
  std::unique_ptr<password_manager::LeakDetectionCheck> leak_check_;

  // A timer to enforce LeakDetection timeouts.
  base::OneShotTimer leak_detection_timer_;

  // Callback to be performed at the end of the leak check.
  Callback callback_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_PASSWORD_CHANGE_SAVE_PASSWORD_LEAK_DETECTION_DELEGATE_H_
