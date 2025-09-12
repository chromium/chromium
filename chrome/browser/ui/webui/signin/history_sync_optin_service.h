// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_SERVICE_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/observer_list_types.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin_helper.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;
struct AccountInfo;

class HistorySyncOptinServiceDefaultDelegate
    : public HistorySyncOptinHelper::Delegate {
 public:
  explicit HistorySyncOptinServiceDefaultDelegate();
  ~HistorySyncOptinServiceDefaultDelegate() override;

  // HistorySyncOptinHelper::Delegate:
  void ShowHistorySyncOptinScreen(Profile* profile) override;
  void ShowAccountManagementScreen(
      signin::SigninChoiceCallback on_account_management_screen_closed)
      override;
  void FinishFlowWithoutHistorySyncOptin() override;
};

// Service responsible for managing the History Sync Opt-in flow.
class HistorySyncOptinService : public KeyedService {
 public:
  explicit HistorySyncOptinService(Profile* profile);
  ~HistorySyncOptinService() override;
  HistorySyncOptinService(const HistorySyncOptinService&) = delete;
  HistorySyncOptinService& operator=(const HistorySyncOptinService&) = delete;

  // Starts the history sync opt-in flow.
  void StartHistorySyncOptinFlow(
      const AccountInfo& account_info,
      std::unique_ptr<HistorySyncOptinHelper::Delegate> delegate);

  void SetDelegateForTesting(HistorySyncOptinHelper::Delegate* delegate);

 private:
  // KeyedService implementation:
  void Shutdown() override;

  std::unique_ptr<HistorySyncOptinHelper::Delegate>
      history_sync_optin_delegate_ = nullptr;
  std::unique_ptr<HistorySyncOptinHelper> history_sync_optin_helper_ = nullptr;
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_SERVICE_H_
