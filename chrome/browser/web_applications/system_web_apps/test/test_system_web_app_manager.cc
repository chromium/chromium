// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/system_web_apps/test/test_system_web_app_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/ash/system_web_apps/system_web_app_manager_factory.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"

namespace web_app {

// static
std::unique_ptr<KeyedService> TestSystemWebAppManager::BuildDefault(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);

  WebAppProvider* provider = WebAppProvider::GetForLocalAppsUnchecked(profile);
  DCHECK(provider);

  auto test_swa_manager = std::make_unique<TestSystemWebAppManager>(profile);

  test_swa_manager->ConnectSubsystems(provider);

  // We don't auto-install system web apps in `TestingProfile`. Tests must
  // opt-in to call `ScheduleStart()` or `Start()` when they need.

  return test_swa_manager;
}

// static
TestSystemWebAppManager* TestSystemWebAppManager::Get(Profile* profile) {
  CHECK(profile->AsTestingProfile());
  auto* test_swa_manager = static_cast<TestSystemWebAppManager*>(
      TestSystemWebAppManager::GetForLocalAppsUnchecked(profile));
  return test_swa_manager;
}

TestSystemWebAppManager::TestSystemWebAppManager(Profile* profile)
    : SystemWebAppManager(profile) {
  SetSystemAppsForTesting(
      base::flat_map<ash::SystemWebAppType,
                     std::unique_ptr<ash::SystemWebAppDelegate>>());
}

TestSystemWebAppManager::~TestSystemWebAppManager() = default;

void TestSystemWebAppManager::SetUpdatePolicy(
    SystemWebAppManager::UpdatePolicy policy) {
  SetUpdatePolicyForTesting(policy);
}

const base::Version& TestSystemWebAppManager::CurrentVersion() const {
  return current_version_;
}

const std::string& TestSystemWebAppManager::CurrentLocale() const {
  return current_locale_;
}

TestSystemWebAppManagerCreator::TestSystemWebAppManagerCreator(
    CreateSystemWebAppManagerCallback callback)
    : callback_(std::move(callback)) {
  create_services_subscription_ =
      BrowserContextDependencyManager::GetInstance()
          ->RegisterCreateServicesCallbackForTesting(
              base::BindRepeating(&TestSystemWebAppManagerCreator::
                                      OnWillCreateBrowserContextServices,
                                  base::Unretained(this)));
}

TestSystemWebAppManagerCreator::~TestSystemWebAppManagerCreator() = default;

void TestSystemWebAppManagerCreator::OnWillCreateBrowserContextServices(
    content::BrowserContext* context) {
  ash::SystemWebAppManagerFactory::GetInstance()->SetTestingFactory(
      context, base::BindRepeating(
                   &TestSystemWebAppManagerCreator::CreateSystemWebAppManager,
                   base::Unretained(this)));
}

std::unique_ptr<KeyedService>
TestSystemWebAppManagerCreator::CreateSystemWebAppManager(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  DCHECK(!ash::SystemWebAppManagerFactory::IsServiceCreatedForProfile(profile));
  if (!AreWebAppsEnabled(profile) || !callback_)
    return nullptr;
  return callback_.Run(profile);
}

}  // namespace web_app
