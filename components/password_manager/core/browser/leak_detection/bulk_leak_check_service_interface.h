// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_BULK_LEAK_CHECK_SERVICE_INTERFACE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_BULK_LEAK_CHECK_SERVICE_INTERFACE_H_

#include <memory>
#include <vector>

#include "base/observer_list_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"

namespace password_manager {

// The interface for a service that allows to check arbitrary number of
// passwords against the database of leaked credentials.
class BulkLeakCheckServiceInterface : public KeyedService {
 public:
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.password_check
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: BulkLeakCheckServiceState
  enum class State {
    // The service is idle and there was no previous error.
    kIdle,
    // The service is checking some credentials.
    kRunning,

    // Those below are error states. On any error the current job is aborted.
    // The error is sticky until next CheckUsernamePasswordPairs() call.

    // Cancel() aborted the running check.
    kCanceled,
    // The user isn't signed-in to Chrome.
    kSignedOut,
    // Error obtaining an access token.
    kTokenRequestFailure,
    // Error in hashing/encrypting for the request.
    kHashingFailure,
    // Error related to network.
    kNetworkError,
    // Error related to the password leak Google service.
    kServiceError,
    // Error related to the quota limit of the password leak Google service.
    kQuotaLimit,
  };

  class Observer : public base::CheckedObserver {
   public:
    // BulkLeakCheckService changed its state.
    virtual void OnStateChanged(State state) = 0;

    // Called when |credential| is analyzed.
    virtual void OnCredentialDone(const LeakCheckCredential& credential,
                                  IsLeaked is_leaked) = 0;

    // Called when the service is shut down.
    virtual void OnBulkCheckServiceShutDown() {}
  };

  BulkLeakCheckServiceInterface();
  ~BulkLeakCheckServiceInterface() override;

  // Not copyable or movable
  BulkLeakCheckServiceInterface(const BulkLeakCheckServiceInterface&) = delete;
  BulkLeakCheckServiceInterface& operator=(
      const BulkLeakCheckServiceInterface&) = delete;
  BulkLeakCheckServiceInterface(BulkLeakCheckServiceInterface&&) = delete;
  BulkLeakCheckServiceInterface& operator=(BulkLeakCheckServiceInterface&&) =
      delete;

  // Starts the checks or appends |credentials| to the existing queue.
  virtual void CheckUsernamePasswordPairs(
      LeakDetectionInitiator initiator,
      std::vector<LeakCheckCredential> credentials) = 0;

  // Stops all the current checks immediately.
  virtual void Cancel() = 0;

  // Returns number of pending passwords to be checked.
  virtual size_t GetPendingChecksCount() const = 0;

  // Returns the current state of the service.
  virtual State GetState() const = 0;

  virtual void AddObserver(Observer* obs) = 0;

  virtual void RemoveObserver(Observer* obs) = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_BULK_LEAK_CHECK_SERVICE_INTERFACE_H_
