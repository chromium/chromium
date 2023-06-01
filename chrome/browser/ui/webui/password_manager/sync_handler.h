// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_SYNC_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_SYNC_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"
#include "content/public/browser/web_ui_message_handler.h"

class Profile;

namespace password_manager {

// WARNING: Keep synced with
// chrome/browser/resources/password_manager/sync_browser_proxy.ts.
enum class TrustedVaultBannerState {
  kNotShown = 0,
  kOfferOptIn = 1,
  kOptedIn = 2,
};

// A class allowing PasswordManager WebUI to interact with the sync
// service and identity manager and observing and propagating relevant
// events to the WebUI.
class SyncHandler : public content::WebUIMessageHandler,
                    public signin::IdentityManager::Observer,
                    public syncer::SyncServiceObserver {
 public:
  explicit SyncHandler(Profile* profile);

  SyncHandler(const SyncHandler&) = delete;
  SyncHandler& operator=(const SyncHandler&) = delete;

  ~SyncHandler() override;

 private:
  // WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // Retrieves the trusted vault banner state value from the SyncService.
  base::Value GetTrustedVaultBannerState() const;
  // Handles the request for the trusted vault banner state.
  void HandleGetTrustedVaultBannerState(const base::Value::List& args);

  // Retrieves sync related information from the SyncService.
  base::Value::Dict GetSyncInfo() const;
  // Handles the request for sync information.
  void HandleGetSyncInfo(const base::Value::List& args);

  // Retrieves information about the primary account.
  base::Value::Dict GetAccountInfo() const;
  // Handles the request for the primary account information.
  void HandleGetAccountInfo(const base::Value::List& args);

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync_service) override;

  // IdentityManager::Observer implementation.
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnExtendedAccountInfoRemoved(const AccountInfo& info) override;

  syncer::SyncService* GetSyncService() const;

  // Weak pointer.
  raw_ptr<Profile, DanglingUntriaged> profile_;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_SYNC_HANDLER_H_
