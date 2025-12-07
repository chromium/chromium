// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_SERVICE_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin_helper.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class Profile;
struct AccountInfo;

class HistorySyncOptinServiceDefaultDelegate
    : public HistorySyncOptinHelper::Delegate {
 public:
  HistorySyncOptinServiceDefaultDelegate();
  ~HistorySyncOptinServiceDefaultDelegate() override;

  // HistorySyncOptinHelper::Delegate:
  void ShowHistorySyncOptinScreen(
      Profile* profile,
      HistorySyncOptinHelper::FlowCompletedCallback callback) override;
  void ShowAccountManagementScreen(
      signin::SigninChoiceCallback on_account_management_screen_closed)
      override;
  void FinishFlowWithoutHistorySyncOptin() override;
};

// Service responsible for managing the History Sync Opt-in flow.
class HistorySyncOptinService : public KeyedService,
                                public HistorySyncOptinHelper::Observer,
                                public signin::IdentityManager::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when the HistorySyncOptinService resets its state.
    virtual void OnHistorySyncOptinServiceReset() {}

   protected:
    ~Observer() override = default;
  };

  explicit HistorySyncOptinService(Profile* profile);
  ~HistorySyncOptinService() override;
  HistorySyncOptinService(const HistorySyncOptinService&) = delete;
  HistorySyncOptinService& operator=(const HistorySyncOptinService&) = delete;

  // Starts the history sync opt-in flow.
  bool StartHistorySyncOptinFlow(
      const AccountInfo& account_info,
      std::unique_ptr<HistorySyncOptinHelper::Delegate> delegate,
      signin_metrics::AccessPoint access_point);

  bool ResumeShowHistorySyncOptinScreenFlowForManagedUser(
      CoreAccountId account_id,
      std::unique_ptr<HistorySyncOptinHelper::Delegate> delegate,
      signin_metrics::AccessPoint access_point);

  base::WeakPtr<HistorySyncOptinService> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  HistorySyncOptinHelper* GetHistorySyncOptinHelperForTesting() {
    return history_sync_optin_helper_.get();
  }

  void SetDelegateForTesting(
      std::unique_ptr<HistorySyncOptinHelper::Delegate> delegate);

 private:
  FRIEND_TEST_ALL_PREFIXES(HistorySyncOptinServiceTest,
                           ShowsManagementScreenThenHistorySyncOnNewProfile);
  FRIEND_TEST_ALL_PREFIXES(HistorySyncOptinServiceTest, MultipleObservers);

  bool Initialize(const AccountInfo& account_info,
                  std::unique_ptr<HistorySyncOptinHelper::Delegate> delegate,
                  signin_metrics::AccessPoint access_point);

  // KeyedService implementation:
  void Shutdown() override;

  void Reset();

  // HistorySyncOptinHelper::Observer implementation:
  void OnHistorySyncOptinHelperFlowFinished() override;

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;

  // Virtual for testing purposes only.
  virtual void ShowErrorDialogWithMessage(int error_message_id);

  std::unique_ptr<HistorySyncOptinHelper::Delegate>
      history_sync_optin_delegate_ = nullptr;

  std::unique_ptr<HistorySyncOptinHelper::Delegate>
      history_sync_optin_delegate_for_testing_ = nullptr;

  std::unique_ptr<HistorySyncOptinHelper> history_sync_optin_helper_ = nullptr;
  raw_ptr<Profile> profile_;

  base::ScopedObservation<HistorySyncOptinHelper,
                          HistorySyncOptinHelper::Observer>
      history_sync_optin_observation_{this};
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_scoped_observation_{this};

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<HistorySyncOptinService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_SERVICE_H_
