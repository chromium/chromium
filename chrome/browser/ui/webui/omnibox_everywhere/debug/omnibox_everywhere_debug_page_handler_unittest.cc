// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_everywhere/debug/omnibox_everywhere_debug_page_handler.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/browser/ui/webui/omnibox_everywhere/debug/omnibox_everywhere_debug.mojom.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace omnibox_everywhere_debug {
namespace {

class TestDebugPage : public mojom::Page {
 public:
  TestDebugPage() = default;
  ~TestDebugPage() override = default;

  mojo::PendingRemote<mojom::Page> BindAndGetRemote() {
    receiver_.reset();
    return receiver_.BindNewPipeAndPassRemote();
  }

  // mojom::Page:
  void OnBackgroundModeChanged(bool enabled) override {}
  void OnHotkeyChanged(bool enabled) override {}
  void OnEphemeralModelChanged(bool enabled) override {}
  void OnLaunchOnStartupChanged(bool enabled) override {}

 private:
  mojo::Receiver<mojom::Page> receiver_{this};
};

class OmniboxEverywhereDebugPageHandlerTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    web_ui_.set_web_contents(web_contents());
  }

  void TearDown() override {
    if (g_browser_process && g_browser_process->local_state()) {
      omnibox_everywhere::prefs::ResetLocalStatePrefs(
          g_browser_process->local_state());
    }
    web_ui_.set_web_contents(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  }

  content::TestWebUI web_ui_;
  TestDebugPage page_;
};

TEST_F(OmniboxEverywhereDebugPageHandlerTest, ResetProfilePrefsClearsPrefs) {
  mojo::Remote<mojom::PageHandler> handler_remote;
  auto handler = std::make_unique<OmniboxEverywhereDebugPageHandler>(
      &web_ui_, profile(), page_.BindAndGetRemote(),
      handler_remote.BindNewPipeAndPassReceiver());

  profile()->GetPrefs()->SetInteger(
      omnibox_everywhere::prefs::kOmniboxEverywhereShowShortcuts,
      std::to_underlying(
          omnibox_everywhere::prefs::ShowShortcutsPrefValue::kDisabled));

  base::test::TestFuture<bool> future;
  handler_remote->ResetProfilePrefs(future.GetCallback());
  EXPECT_TRUE(future.Get());

  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                omnibox_everywhere::prefs::kOmniboxEverywhereShowShortcuts),
            std::to_underlying(
                omnibox_everywhere::prefs::ShowShortcutsPrefValue::kUnset));
}

// Regression test for crbug.com/579037685: resetting profile preferences can
// synchronously notify observers (such as `OmniboxEverywhereUIManager`) that
// tear down the hosting WebUI and destroy `OmniboxEverywhereDebugPageHandler`
// before `ResetProfilePrefs()` returns.
TEST_F(OmniboxEverywhereDebugPageHandlerTest,
       ResetProfilePrefsSurvivesSynchronousDestruction) {
  mojo::Remote<mojom::PageHandler> handler_remote;
  auto handler = std::make_unique<OmniboxEverywhereDebugPageHandler>(
      &web_ui_, profile(), page_.BindAndGetRemote(),
      handler_remote.BindNewPipeAndPassReceiver());
  base::RunLoop disconnect_loop;
  handler_remote.set_disconnect_handler(disconnect_loop.QuitClosure());

  profile()->GetPrefs()->SetInteger(
      omnibox_everywhere::prefs::kOmniboxEverywhereShowShortcuts,
      std::to_underlying(
          omnibox_everywhere::prefs::ShowShortcutsPrefValue::kDisabled));

  bool destroyed_synchronously = false;
  PrefChangeRegistrar registrar;
  registrar.Init(profile()->GetPrefs());
  registrar.Add(omnibox_everywhere::prefs::kOmniboxEverywhereShowShortcuts,
                base::BindLambdaForTesting([&]() {
                  handler.reset();
                  destroyed_synchronously = true;
                }));

  handler_remote->ResetProfilePrefs(base::BindOnce([](bool) {}));
  disconnect_loop.Run();

  EXPECT_TRUE(destroyed_synchronously);
  EXPECT_FALSE(handler);
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                omnibox_everywhere::prefs::kOmniboxEverywhereShowShortcuts),
            std::to_underlying(
                omnibox_everywhere::prefs::ShowShortcutsPrefValue::kUnset));
}

TEST_F(OmniboxEverywhereDebugPageHandlerTest,
       ResetAllPrefsSurvivesSynchronousDestructionOnProfilePrefChange) {
  mojo::Remote<mojom::PageHandler> handler_remote;
  auto handler = std::make_unique<OmniboxEverywhereDebugPageHandler>(
      &web_ui_, profile(), page_.BindAndGetRemote(),
      handler_remote.BindNewPipeAndPassReceiver());
  base::RunLoop disconnect_loop;
  handler_remote.set_disconnect_handler(disconnect_loop.QuitClosure());

  profile()->GetPrefs()->SetInteger(
      omnibox_everywhere::prefs::kOmniboxEverywhereShowShortcuts,
      std::to_underlying(
          omnibox_everywhere::prefs::ShowShortcutsPrefValue::kDisabled));

  bool destroyed_synchronously = false;
  PrefChangeRegistrar registrar;
  registrar.Init(profile()->GetPrefs());
  registrar.Add(omnibox_everywhere::prefs::kOmniboxEverywhereShowShortcuts,
                base::BindLambdaForTesting([&]() {
                  handler.reset();
                  destroyed_synchronously = true;
                }));

  handler_remote->ResetAllPrefs(base::BindOnce([](bool) {}));
  disconnect_loop.Run();

  EXPECT_TRUE(destroyed_synchronously);
  EXPECT_FALSE(handler);
}

TEST_F(OmniboxEverywhereDebugPageHandlerTest,
       ResetAllPrefsSurvivesSynchronousDestructionOnLocalStatePrefChange) {
  PrefService* local_state = g_browser_process->local_state();
  ASSERT_TRUE(local_state);
  const bool default_ephemeral = local_state->GetBoolean(
      omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel);
  local_state->SetBoolean(
      omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel,
      !default_ephemeral);

  mojo::Remote<mojom::PageHandler> handler_remote;
  auto handler = std::make_unique<OmniboxEverywhereDebugPageHandler>(
      &web_ui_, profile(), page_.BindAndGetRemote(),
      handler_remote.BindNewPipeAndPassReceiver());
  base::RunLoop disconnect_loop;
  handler_remote.set_disconnect_handler(disconnect_loop.QuitClosure());

  bool destroyed_synchronously = false;
  PrefChangeRegistrar registrar;
  registrar.Init(local_state);
  registrar.Add(omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel,
                base::BindLambdaForTesting([&]() {
                  handler.reset();
                  destroyed_synchronously = true;
                }));

  handler_remote->ResetAllPrefs(base::BindOnce([](bool) {}));
  disconnect_loop.Run();

  EXPECT_TRUE(destroyed_synchronously);
  EXPECT_FALSE(handler);
  EXPECT_EQ(local_state->GetBoolean(
                omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel),
            default_ephemeral);
}

TEST_F(OmniboxEverywhereDebugPageHandlerTest,
       ShowLensIphDoesNotCrashWithoutTabWebContents) {
  mojo::Remote<mojom::PageHandler> handler_remote_non_tab;
  auto handler_non_tab = std::make_unique<OmniboxEverywhereDebugPageHandler>(
      &web_ui_, profile(), page_.BindAndGetRemote(),
      handler_remote_non_tab.BindNewPipeAndPassReceiver());
  handler_non_tab->ShowLensIph();

  TestDebugPage second_page;
  mojo::Remote<mojom::PageHandler> handler_remote_null_webui;
  auto handler_null_webui = std::make_unique<OmniboxEverywhereDebugPageHandler>(
      /*web_ui=*/nullptr, profile(), second_page.BindAndGetRemote(),
      handler_remote_null_webui.BindNewPipeAndPassReceiver());
  handler_null_webui->ShowLensIph();
}

TEST(OmniboxEverywhereDebugPageHandlerProfileManagerTest,
     ResetAllPrefsWithLoadedProfilesSurvivesSynchronousDestruction) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());
  TestingProfile* loaded_profile =
      profile_manager.CreateTestingProfile("Default");
  ASSERT_TRUE(loaded_profile);

  loaded_profile->GetPrefs()->SetInteger(
      omnibox_everywhere::prefs::kOmniboxEverywhereShowShortcuts,
      std::to_underlying(
          omnibox_everywhere::prefs::ShowShortcutsPrefValue::kDisabled));

  TestDebugPage page;
  mojo::Remote<mojom::PageHandler> handler_remote;
  auto handler = std::make_unique<OmniboxEverywhereDebugPageHandler>(
      /*web_ui=*/nullptr, loaded_profile, page.BindAndGetRemote(),
      handler_remote.BindNewPipeAndPassReceiver());
  base::RunLoop disconnect_loop;
  handler_remote.set_disconnect_handler(disconnect_loop.QuitClosure());

  bool destroyed_synchronously = false;
  PrefChangeRegistrar registrar;
  registrar.Init(loaded_profile->GetPrefs());
  registrar.Add(omnibox_everywhere::prefs::kOmniboxEverywhereShowShortcuts,
                base::BindLambdaForTesting([&]() {
                  handler.reset();
                  destroyed_synchronously = true;
                }));

  handler_remote->ResetAllPrefs(base::BindOnce([](bool) {}));
  disconnect_loop.Run();

  EXPECT_TRUE(destroyed_synchronously);
  EXPECT_FALSE(handler);
  EXPECT_EQ(loaded_profile->GetPrefs()->GetInteger(
                omnibox_everywhere::prefs::kOmniboxEverywhereShowShortcuts),
            std::to_underlying(
                omnibox_everywhere::prefs::ShowShortcutsPrefValue::kUnset));
}

}  // namespace
}  // namespace omnibox_everywhere_debug
