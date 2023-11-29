// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_FAKE_BULK_LEAK_CHECK_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_FAKE_BULK_LEAK_CHECK_SERVICE_H_

#import "base/observer_list.h"
#import "components/password_manager/core/browser/leak_detection/bulk_leak_check_service_interface.h"

namespace password_manager {

// Fake version of the BulkLeakCheckService. Used for EG tests.
class FakeBulkLeakCheckService : public BulkLeakCheckServiceInterface {
 public:
  FakeBulkLeakCheckService();
  ~FakeBulkLeakCheckService() override;

  // BulkLeakCheckServiceInterface
  void CheckUsernamePasswordPairs(
      LeakDetectionInitiator initiator,
      std::vector<LeakCheckCredential> credentials) override;
  void Cancel() override;
  size_t GetPendingChecksCount() const override;
  State GetState() const override;
  void AddObserver(Observer* obs) override;
  void RemoveObserver(Observer* obs) override;

  // Setter for the `buffered_state_` variable.
  void SetBufferedState(State state);

 private:
  // Notify the observers of a change in state.
  void NotifyStateChanged();

  // Sets `state_` to `buffered_state_` and notifies the observers.
  void SetStateToBufferedState();

  // The service's state.
  State state_ = State::kIdle;

  // The state that the service should be in when the leak check finishes. Used
  // to simulate the completion of a real leak check.
  State buffered_state_ = State::kIdle;

  // The list of observers observing the state of the service.
  base::ObserverList<Observer> observers_;

  // Weak pointer for this class. Used to schedule a task.
  base::WeakPtrFactory<FakeBulkLeakCheckService> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_FAKE_BULK_LEAK_CHECK_SERVICE_H_
