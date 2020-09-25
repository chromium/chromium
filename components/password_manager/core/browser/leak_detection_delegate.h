// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_DELEGATE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_DELEGATE_H_

#include <memory>
#include <utility>

#include "base/timer/elapsed_timer.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check_factory.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/password_form_forward.h"

class PrefService;

namespace password_manager {

class LeakDetectionCheck;
class LeakDetectionDelegateHelper;
class PasswordManagerClient;

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

  void StartLeakCheck(const PasswordForm& form);

 private:
  // LeakDetectionDelegateInterface:
  void OnLeakDetectionDone(bool is_leaked,
                           GURL url,
                           base::string16 username,
                           base::string16 password) override;

  // Initiates the showing of the leak detection notification. It is called by
  // |helper_| after |is_saved|/|is_reused| was asynchronously determined.
  void OnShowLeakDetectionNotification(IsSaved is_saved,
                                       IsReused is_reused,
                                       GURL url,
                                       base::string16 username,
                                       CompromisedSitesCount saved_sites);

  void OnError(LeakDetectionError error) override;

  PasswordManagerClient* client_;
  // The factory that creates objects for performing a leak check up.
  std::unique_ptr<LeakDetectionCheckFactory> leak_factory_;

  // Current leak check-up being performed in the background.
  std::unique_ptr<LeakDetectionCheck> leak_check_;

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
