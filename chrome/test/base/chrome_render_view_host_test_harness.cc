// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_render_view_host_test_harness.h"

#include <utility>

#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/signin_error_controller.h"

#if defined(OS_CHROMEOS)
#include "ash/shell.h"
#endif

using content::RenderViewHostTester;
using content::RenderViewHostTestHarness;

namespace {

std::unique_ptr<KeyedService> BuildSigninManagerFake(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  SigninClient* signin_client =
      ChromeSigninClientFactory::GetForProfile(profile);
  AccountTrackerService* account_tracker_service =
      AccountTrackerServiceFactory::GetForProfile(profile);
  SigninErrorController* signin_error_controller =
      SigninErrorControllerFactory::GetForProfile(profile);
#if defined (OS_CHROMEOS)
  std::unique_ptr<SigninManagerBase> signin(new SigninManagerBase(
      signin_client, account_tracker_service, signin_error_controller));
  signin->Initialize(NULL);
  return std::move(signin);
#else
  std::unique_ptr<FakeSigninManager> manager(new FakeSigninManager(
      signin_client, ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
      account_tracker_service,
      GaiaCookieManagerServiceFactory::GetForProfile(profile),
      signin_error_controller));
  manager->Initialize(g_browser_process->local_state());
  return std::move(manager);
#endif
}

}  // namespace

ChromeRenderViewHostTestHarness::ChromeRenderViewHostTestHarness(
    int thread_bundle_options)
    : content::RenderViewHostTestHarness(thread_bundle_options) {}

ChromeRenderViewHostTestHarness::~ChromeRenderViewHostTestHarness() = default;

TestingProfile* ChromeRenderViewHostTestHarness::profile() {
  return static_cast<TestingProfile*>(browser_context());
}

void ChromeRenderViewHostTestHarness::TearDown() {
  RenderViewHostTestHarness::TearDown();
#if defined(OS_CHROMEOS)
  ash::Shell::DeleteInstance();
#endif
}

content::BrowserContext*
ChromeRenderViewHostTestHarness::CreateBrowserContext() {
  TestingProfile::Builder builder;
  builder.AddTestingFactory(SigninManagerFactory::GetInstance(),
                            base::BindRepeating(&BuildSigninManagerFake));
  return builder.Build().release();
}
