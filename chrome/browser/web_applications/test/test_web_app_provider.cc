// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_web_app_provider.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_shortcut_manager.h"
#include "chrome/browser/web_applications/components/file_handler_manager.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/components/web_app_ui_manager.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/test/test_system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace web_app {

// static
std::unique_ptr<KeyedService> TestWebAppProvider::BuildDefault(
    content::BrowserContext* context) {
  auto provider = std::make_unique<TestWebAppProvider>(
      Profile::FromBrowserContext(context),
      /*run_subsystem_startup_tasks=*/false);
  // TODO(crbug.com/973324): Replace core subsystems with fakes by default.
  provider->ConnectSubsystems();
  return provider;
}

// static
TestWebAppProvider* TestWebAppProvider::Get(Profile* profile) {
  CHECK(profile->AsTestingProfile());
  auto* test_provider =
      static_cast<TestWebAppProvider*>(WebAppProvider::Get(profile));
  CHECK(!test_provider->started_);

  // Disconnect so that clients are forced to call Start() before accessing any
  // subsystems.
  test_provider->connected_ = false;

  return test_provider;
}

TestWebAppProvider::TestWebAppProvider(Profile* profile,
                                       bool run_subsystem_startup_tasks)
    : WebAppProvider(profile),
      run_subsystem_startup_tasks_(run_subsystem_startup_tasks) {}

TestWebAppProvider::~TestWebAppProvider() = default;

void TestWebAppProvider::SetRegistrar(std::unique_ptr<AppRegistrar> registrar) {
  CheckNotStarted();
  registrar_ = std::move(registrar);
}

void TestWebAppProvider::SetFileHandlerManager(
    std::unique_ptr<FileHandlerManager> file_handler_manager) {
  CheckNotStarted();
  file_handler_manager_ = std::move(file_handler_manager);
}

void TestWebAppProvider::SetInstallManager(
    std::unique_ptr<WebAppInstallManager> install_manager) {
  CheckNotStarted();
  install_manager_ = std::move(install_manager);
}

void TestWebAppProvider::SetInstallFinalizer(
    std::unique_ptr<InstallFinalizer> install_finalizer) {
  CheckNotStarted();
  install_finalizer_ = std::move(install_finalizer);
}

void TestWebAppProvider::SetPendingAppManager(
    std::unique_ptr<PendingAppManager> pending_app_manager) {
  CheckNotStarted();
  pending_app_manager_ = std::move(pending_app_manager);
}

void TestWebAppProvider::SetWebAppUiManager(
    std::unique_ptr<WebAppUiManager> ui_manager) {
  CheckNotStarted();
  ui_manager_ = std::move(ui_manager);
}

void TestWebAppProvider::SetSystemWebAppManager(
    std::unique_ptr<SystemWebAppManager> system_web_app_manager) {
  CheckNotStarted();
  system_web_app_manager_ = std::move(system_web_app_manager);
}

void TestWebAppProvider::SetWebAppPolicyManager(
    std::unique_ptr<WebAppPolicyManager> web_app_policy_manager) {
  CheckNotStarted();
  web_app_policy_manager_ = std::move(web_app_policy_manager);
}

void TestWebAppProvider::SetShortcutManager(
    std::unique_ptr<AppShortcutManager> shortcut_manager) {
  CheckNotStarted();
  shortcut_manager_ = std::move(shortcut_manager);
}

void TestWebAppProvider::CheckNotStarted() const {
  CHECK(!started_) << "Attempted to set a WebAppProvider subsystem after "
                      "Start() was called.";
}

void TestWebAppProvider::StartImpl() {
  if (run_subsystem_startup_tasks_)
    WebAppProvider::StartImpl();
}

TestWebAppProviderCreator::TestWebAppProviderCreator(
    CreateWebAppProviderCallback callback)
    : callback_(std::move(callback)) {
  will_create_browser_context_services_subscription_ =
      BrowserContextDependencyManager::GetInstance()
          ->RegisterWillCreateBrowserContextServicesCallbackForTesting(
              base::BindRepeating(&TestWebAppProviderCreator::
                                      OnWillCreateBrowserContextServices,
                                  base::Unretained(this)));
}

TestWebAppProviderCreator::~TestWebAppProviderCreator() = default;

void TestWebAppProviderCreator::OnWillCreateBrowserContextServices(
    content::BrowserContext* context) {
  WebAppProviderFactory::GetInstance()->SetTestingFactory(
      context,
      base::BindRepeating(&TestWebAppProviderCreator::CreateWebAppProvider,
                          base::Unretained(this)));
}

std::unique_ptr<KeyedService> TestWebAppProviderCreator::CreateWebAppProvider(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  if (!AreWebAppsEnabled(profile) || !callback_)
    return nullptr;
  return std::move(callback_).Run(profile);
}

}  // namespace web_app
