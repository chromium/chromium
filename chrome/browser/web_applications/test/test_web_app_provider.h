// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_WEB_APP_PROVIDER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_WEB_APP_PROVIDER_H_

#include <memory>

#include "base/callback.h"
#include "base/callback_list.h"
#include "chrome/browser/web_applications/web_app_provider.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace web_app {

class AppRegistrar;
class AppShortcutManager;
class InstallFinalizer;
class PendingAppManager;
class SystemWebAppManager;
class WebAppInstallManager;
class WebAppPolicyManager;

class TestWebAppProvider : public WebAppProvider {
 public:
  // Used by the TestingProfile in unit tests.
  // Builds a stub WebAppProvider which will not fire subsystem startup tasks.
  // Use TestWebAppProvider::Get() to replace subsystems.
  static std::unique_ptr<KeyedService> BuildDefault(
      content::BrowserContext* context);

  // Gets a TestWebAppProvider that can have its subsystems set. This should
  // only be called once during SetUp(), and clients must call Start() before
  // using the subsystems.
  static TestWebAppProvider* Get(Profile* profile);

  // |run_subsystem_startup_tasks| is true by default as browser test clients
  // will generally want to construct their TestWebAppProvider to behave as it
  // would in a production browser.
  explicit TestWebAppProvider(Profile* profile,
                              bool run_subsystem_startup_tasks = true);
  ~TestWebAppProvider() override;

  void SetRegistrar(std::unique_ptr<AppRegistrar> registrar);
  void SetFileHandlerManager(
      std::unique_ptr<FileHandlerManager> file_handler_manager);
  void SetInstallManager(std::unique_ptr<WebAppInstallManager> install_manager);
  void SetInstallFinalizer(std::unique_ptr<InstallFinalizer> install_finalizer);
  void SetPendingAppManager(
      std::unique_ptr<PendingAppManager> pending_app_manager);
  void SetWebAppUiManager(std::unique_ptr<WebAppUiManager> ui_manager);
  void SetSystemWebAppManager(
      std::unique_ptr<SystemWebAppManager> system_web_app_manager);
  void SetWebAppPolicyManager(
      std::unique_ptr<WebAppPolicyManager> web_app_policy_manager);
  void SetShortcutManager(std::unique_ptr<AppShortcutManager> shortcut_manager);

 private:
  void CheckNotStarted() const;

  // WebAppProvider:
  void StartImpl() override;

  // If true, when Start()ed the TestWebAppProvider will call
  // WebAppProvider::StartImpl() and fire startup tasks like a real
  // WebAppProvider.
  bool run_subsystem_startup_tasks_;
};

// Used in BrowserTests to ensure that the WebAppProvider that is create on
// profile startup is the TestWebAppProvider. Hooks into the
// BrowserContextKeyedService initialization pipeline.
class TestWebAppProviderCreator {
 public:
  using CreateWebAppProviderCallback =
      base::OnceCallback<std::unique_ptr<KeyedService>(Profile* profile)>;

  explicit TestWebAppProviderCreator(CreateWebAppProviderCallback callback);
  ~TestWebAppProviderCreator();

 private:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context);
  std::unique_ptr<KeyedService> CreateWebAppProvider(
      content::BrowserContext* context);

  CreateWebAppProviderCallback callback_;

  std::unique_ptr<
      base::CallbackList<void(content::BrowserContext*)>::Subscription>
      will_create_browser_context_services_subscription_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_WEB_APP_PROVIDER_H_
