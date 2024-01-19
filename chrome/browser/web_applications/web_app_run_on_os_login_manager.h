// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_RUN_ON_OS_LOGIN_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_RUN_ON_OS_LOGIN_MANAGER_H_

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "services/network/public/cpp/network_connection_tracker.h"

class Profile;

namespace web_app {

class AllAppsLock;
class WebAppProvider;

// This class runs web apps on OS Login on ChromeOS once the corresponding
// policy has been read by the WebAppPolicyManager.
class WebAppRunOnOsLoginManager
    : public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  explicit WebAppRunOnOsLoginManager(Profile* profile);
  WebAppRunOnOsLoginManager(const WebAppRunOnOsLoginManager&) = delete;
  WebAppRunOnOsLoginManager& operator=(const WebAppRunOnOsLoginManager&) =
      delete;
  ~WebAppRunOnOsLoginManager() override;

  void SetProvider(base::PassKey<WebAppProvider>, WebAppProvider& provider);

  void Start();

  base::WeakPtr<WebAppRunOnOsLoginManager> GetWeakPtr();

  static base::AutoReset<bool> SkipStartupForTesting();
  void RunAppsOnOsLoginForTesting();
  void SetCompletedClosureForTesting(base::OnceClosure completed_closure);

 private:
  void RunAppsOnOsLogin(AllAppsLock& lock, base::Value::Dict& debug_value);

  void OnInitialConnectionTypeReceived(network::mojom::ConnectionType type);

  // network::NetworkConnectionTracker::NetworkConnectionObserver
  // implementation. Observes network change events.
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  void RunOsLoginAppsAndMaybeUnregisterObserver();

  bool scheduled_run_on_os_login_command_ = false;
  base::OnceClosure completed_closure_ = base::DoNothing();
  raw_ptr<WebAppProvider> provider_ = nullptr;
  const raw_ptr<Profile> profile_;

  base::WeakPtrFactory<WebAppRunOnOsLoginManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_RUN_ON_OS_LOGIN_MANAGER_H_
