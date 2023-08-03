// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_apply_waiter.h"

#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

class IsolatedWebAppUpdateApplyWaiterTest : public WebAppTest {
 protected:
  size_t CountProfileKeepAlives(Profile* profile) {
    return profile_manager()
        .profile_manager()
        ->GetKeepAlivesByPath(profile->GetPath())
        .size();
  }

  FakeWebAppUiManager& ui_manager() {
    return static_cast<FakeWebAppUiManager&>(fake_provider().GetUiManager());
  }

  IsolatedWebAppUrlInfo url_info_ =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
          *web_package::SignedWebBundleId::Create(kTestEd25519WebBundleId));
};

TEST_F(IsolatedWebAppUpdateApplyWaiterTest, AwaitsWindowsClosed) {
  test::AwaitStartWebAppProviderAndSubsystems(profile());
  // There is one keep alive by default that waits for the first browser window
  // to open.
  EXPECT_EQ(CountProfileKeepAlives(profile()), 1ul);
  ui_manager().SetNumWindowsForApp(url_info_.app_id(), 1);

  IsolatedWebAppUpdateApplyWaiter waiter(url_info_,
                                         fake_provider().ui_manager());
  EXPECT_EQ(CountProfileKeepAlives(profile()), 1ul);

  base::MockRepeatingCallback<void(AppId)> callback;
  EXPECT_CALL(callback, Run(url_info_.app_id()))
      .WillOnce(::testing::Invoke(
          [&]() { ui_manager().SetNumWindowsForApp(url_info_.app_id(), 0); }));
  ui_manager().SetOnNotifyOnAllAppWindowsClosedCallback(callback.Get());

  base::test::TestFuture<std::unique_ptr<ScopedKeepAlive>,
                         std::unique_ptr<ScopedProfileKeepAlive>>
      future;
  waiter.Wait(profile(), future.GetCallback());
  EXPECT_EQ(CountProfileKeepAlives(profile()), 2ul);
  EXPECT_FALSE(future.IsReady());
  auto [keep_alive, profile_keep_alive] = future.Take();
  EXPECT_NE(keep_alive, nullptr);
  EXPECT_NE(profile_keep_alive, nullptr);

  EXPECT_EQ(CountProfileKeepAlives(profile()), 2ul);
}

TEST_F(IsolatedWebAppUpdateApplyWaiterTest, NeverSynchronouslyCallsCallback) {
  test::AwaitStartWebAppProviderAndSubsystems(profile());

  ui_manager().SetNumWindowsForApp(url_info_.app_id(), 0);

  IsolatedWebAppUpdateApplyWaiter waiter(url_info_,
                                         fake_provider().ui_manager());

  base::MockRepeatingCallback<void(AppId)> callback;
  EXPECT_CALL(callback, Run(url_info_.app_id()));
  ui_manager().SetOnNotifyOnAllAppWindowsClosedCallback(callback.Get());

  base::test::TestFuture<std::unique_ptr<ScopedKeepAlive>,
                         std::unique_ptr<ScopedProfileKeepAlive>>
      future;
  waiter.Wait(profile(), future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  auto [keep_alive, profile_keep_alive] = future.Take();
}

}  // namespace
}  // namespace web_app
