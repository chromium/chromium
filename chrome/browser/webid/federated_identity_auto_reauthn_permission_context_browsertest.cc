// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_auto_reauthn_permission_context.h"

#include "base/test/bind.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/ui/mocks/mock_event_dispatcher.h"
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
  FederatedIdentityAutoReauthnPermissionContextTest() = default;
  FederatedIdentityAutoReauthnPermissionContextTest(
      const FederatedIdentityAutoReauthnPermissionContextTest&) = delete;
  FederatedIdentityAutoReauthnPermissionContextTest& operator=(
      const FederatedIdentityAutoReauthnPermissionContextTest&) = delete;

  ~FederatedIdentityAutoReauthnPermissionContextTest() override = default;
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
  std::unique_ptr<actor::ExecutionEngine> execution_engine =
      std::make_unique<actor::ExecutionEngine>(browser()->profile());

  std::unique_ptr<actor::ActorTask> actor_task =
      std::make_unique<actor::ActorTask>(
          browser()->profile(), std::move(execution_engine),
          actor::ui::NewUiEventDispatcher(
              actor_service->GetActorUiStateManager()));
  actor_task->SetState(actor::ActorTask::State::kActing);

  base::RunLoop loop;
  actor_task->AddTab(
      browser()->GetActiveTabInterface()->GetHandle(),
      base::BindLambdaForTesting([&](actor::mojom::ActionResultPtr result) {
        EXPECT_TRUE(actor::IsOk(*result));
        loop.Quit();
      }));
  loop.Run();
  actor_service->AddActiveTask(std::move(actor_task));

  EXPECT_TRUE(
      FederatedIdentityAutoReauthnPermissionContextFactory::GetForProfile(
          browser()->profile())
          ->IsAutoReauthnDisabledByEmbedder(web_contents));
}

}  // namespace
