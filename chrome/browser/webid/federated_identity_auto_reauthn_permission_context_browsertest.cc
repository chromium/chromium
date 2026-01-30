// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_auto_reauthn_permission_context.h"

#include "base/test/bind.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/ui/test_support/mock_event_dispatcher.h"
#include "chrome/browser/password_manager/password_manager_settings_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/webid/federated_identity_auto_reauthn_permission_context_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"

namespace {

class FederatedIdentityAutoReauthnPermissionContextTest
    : public InProcessBrowserTest {
 public:
  FederatedIdentityAutoReauthnPermissionContextTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{features::kGlicActor,
                               {{features::kGlicActorPolicyControlExemption
                                     .name,
                                 "true"}}}},
        /*disabled_features=*/{});
  }
  FederatedIdentityAutoReauthnPermissionContextTest(
      const FederatedIdentityAutoReauthnPermissionContextTest&) = delete;
  FederatedIdentityAutoReauthnPermissionContextTest& operator=(
      const FederatedIdentityAutoReauthnPermissionContextTest&) = delete;

  ~FederatedIdentityAutoReauthnPermissionContextTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests PasswordManagerSettingsService correctly hooks itself as a cyclic
// dependency. Regression test for crbug.com/428112191.
IN_PROC_BROWSER_TEST_F(FederatedIdentityAutoReauthnPermissionContextTest,
                       AutoReauthnSettingEnabledByDefault) {
  // Force PasswordManagerSettingsService instantiation.
  PasswordManagerSettingsServiceFactory::GetForProfile(browser()->profile());

  EXPECT_TRUE(
      FederatedIdentityAutoReauthnPermissionContextFactory::GetForProfile(
          browser()->profile())
          ->IsAutoReauthnSettingEnabled());
}

// Tests that `IsAutoReauthnDisabledByEmbedder` returns `true`
// when an actor task is active in the `web_contents`.
IN_PROC_BROWSER_TEST_F(FederatedIdentityAutoReauthnPermissionContextTest,
                       AutoReauthnBlockedByActor) {
  auto* const web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Create actor task and attach it to the current tab.
  auto* actor_service = actor::ActorKeyedService::Get(browser()->profile());
  actor::TaskId task_id = actor_service->CreateTask();
  actor::ActorTask* actor_task = actor_service->GetTask(task_id);

  actor_task->SetState(actor::ActorTask::State::kActing);

  base::RunLoop loop;
  actor_task->AddTab(
      browser()->GetActiveTabInterface()->GetHandle(),
      base::BindLambdaForTesting([&](actor::mojom::ActionResultPtr result) {
        EXPECT_TRUE(actor::IsOk(*result));
        loop.Quit();
      }));
  loop.Run();

  EXPECT_TRUE(
      FederatedIdentityAutoReauthnPermissionContextFactory::GetForProfile(
          browser()->profile())
          ->IsAutoReauthnDisabledByEmbedder(web_contents));
}

}  // namespace
