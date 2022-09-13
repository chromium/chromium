// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_DELEGATE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_DELEGATE_H_

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check_factory.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"

class PrefService;

namespace password_manager {

class LeakDetectionCheck;
class LeakDetectionDelegateHelper;
class PasswordManagerClient;
struct PasswordForm;

// The helper class that encapsulates the requests and their processing.
class LeakDetectionDelegate : public LeakDetectionDelegateInterface {
 public:
  explicit LeakDetectionDelegate(PasswordManagerClient* client);
  ~LeakDetectionDelegate() override;

  // Not copyable or movable
  LeakDetectionDelegate(const LeakDetectionDelegate&) = delete;
  LeakDetectionDelegate& operator=(const LeakDetectionDelegate&) = delete;
  LeakDetectionDelegate(LeakDetectionDelegate&&) = delete;
  LeakDetectionDelegate& operator=(LeakDetectionDelegate&&) = delete;

#if defined(UNIT_TEST)
  void set_leak_factory(std::unique_ptr<LeakDetectionCheckFactory> factory) {
    leak_factory_ = std::move(factory);
  }

  LeakDetectionCheck* leak_check() const { return leak_check_.get(); }
#endif  // defined(UNIT_TEST)

  // Starts a leak check for `credentials`. Note that
  // `submitted_form_was_likely_signup_form` is typically derived from a
  // different PasswordForm instance!
  void StartLeakCheck(const PasswordForm& credentials,
                      bool submitted_form_was_likely_signup_form);

 private:
  // LeakDetectionDelegateInterface:
  void OnLeakDetectionDone(bool is_leaked,
                           GURL url,
                           std::u16string username,
                           std::u16string password) override;

  // Initiates the showing of the leak detection notification. It is called by
  // |helper_| after |is_saved|, |is_reused|, and |has_change_script| were
  // asynchronously determined.
  // |all_urls_with_leaked_credentials| contains all the URLs on which the
  // leaked username/password pair is used.
  void OnShowLeakDetectionNotification(
      IsSaved is_saved,
      IsReused is_reused,
      HasChangeScript has_change_script,
      GURL url,
      std::u16string username,
      std::vector<GURL> all_urls_with_leaked_credentials);

  void OnError(LeakDetectionError error) override;

  raw_ptr<PasswordManagerClient> client_;
  // The factory that creates objects for performing a leak check up.
  std::unique_ptr<LeakDetectionCheckFactory> leak_factory_;

  // Current leak check-up being performed in the background.
  std::unique_ptr<LeakDetectionCheck> leak_check_;

  // Whether the form that was submitted was (likely) a signup form.
  bool is_likely_signup_form_ = false;

  // Timer measuring the time it takes from StartLeakCheck() until a call to
  // OnLeakDetectionDone() with is_leaked = true.
  std::unique_ptr<base::ElapsedTimer> is_leaked_timer_;

  // Helper class to asynchronously determine |CredentialLeakType| for leaked
  // credentials.
  std::unique_ptr<LeakDetectionDelegateHelper> helper_;
};

// Determines whether the leak check can be started depending on |prefs|. Will
// use |client| for logging if non-null.
bool CanStartLeakCheck(const PrefService& prefs,
                       const PasswordManagerClient* client = nullptr);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_DELEGATE_H_
