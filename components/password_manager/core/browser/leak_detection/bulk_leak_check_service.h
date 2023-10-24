// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_BULK_LEAK_CHECK_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_BULK_LEAK_CHECK_SERVICE_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check_service_interface.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check_factory.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace password_manager {

// The service that allows to check arbitrary number of passwords against the
// database of leaked credentials.
class BulkLeakCheckService : public BulkLeakCheckDelegateInterface,
                             public BulkLeakCheckServiceInterface {
 public:
  BulkLeakCheckService(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~BulkLeakCheckService() override;

  // Starts the checks or appends |credentials| to the existing queue.
  void CheckUsernamePasswordPairs(
      LeakDetectionInitiator initiator,
      std::vector<LeakCheckCredential> credentials) override;

  // Stops all the current checks immediately.
  void Cancel() override;

  // Returns number of pending passwords to be checked.
  size_t GetPendingChecksCount() const override;

  // Returns the current state of the service.
  State GetState() const override;

  void AddObserver(Observer* obs) override;

  void RemoveObserver(Observer* obs) override;

  // KeyedService:
  void Shutdown() override;

#if defined(UNIT_TEST)
  void set_leak_factory(std::unique_ptr<LeakDetectionCheckFactory> factory) {
    leak_check_factory_ = std::move(factory);
  }

  void set_state_and_notify(State state) {
    state_ = state;
    NotifyStateChanged();
  }
#endif  // defined(UNIT_TEST)

 private:
  class MetricsReporter;
  // BulkLeakCheckDelegateInterface:
  void OnFinishedCredential(LeakCheckCredential credential,
                            IsLeaked is_leaked) override;
  void OnError(LeakDetectionError error) override;

  // Notify the observers.
  void NotifyStateChanged();

  raw_ptr<signin::IdentityManager> identity_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Factory to create |bulk_leak_check_|.
  std::unique_ptr<LeakDetectionCheckFactory> leak_check_factory_;
  // Currently running check.
  std::unique_ptr<BulkLeakCheck> bulk_leak_check_;
  // Reports metrics about bulk leak check.
  std::unique_ptr<MetricsReporter> metrics_reporter_;

  State state_ = State::kIdle;

  base::ObserverList<Observer> observers_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_BULK_LEAK_CHECK_SERVICE_H_
