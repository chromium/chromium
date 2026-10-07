// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/default_browser/default_browser_modal_handler.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/default_browser/default_browser_controller.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_surface_manager.h"
#include "chrome/browser/ui/webui/default_browser/default_browser_modal.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kStickyRetryCountHistogram[] =
    "DefaultBrowser.StickyModalDialogWithoutSettingsIllustration."
    "ShellIntegration.RetryCount";

class StickyTestSurfaceManager : public DefaultBrowserSurfaceManager {
 public:
  StickyTestSurfaceManager() = default;
  ~StickyTestSurfaceManager() override = default;

  default_browser::DefaultBrowserEntrypointType GetEntrypointType()
      const override {
    return default_browser::DefaultBrowserEntrypointType::
        kStickyModalDialogWithoutSettingsIllustration;
  }

  void ShowForBrowser(BrowserWindowInterface* browser) override {}
  void CloseForBrowser(BrowserWindowInterface* browser) override {}
  void CloseAllPromptInstances() override {}
};

class TestDefaultBrowserModalHandler : public DefaultBrowserModalHandler {
 public:
  using DefaultBrowserModalHandler::DefaultBrowserModalHandler;

 protected:
  void LaunchSettings() override {}
};

class DefaultBrowserModalHandlerTest : public testing::Test {
 protected:
  void SetUp() override {
    // Stops the setter run by `HandleAccept()` from opening Windows Settings.
    shell_integration::DefaultBrowserWorker::DisableSetAsDefaultForTesting();
  }

  void TearDown() override {
    DefaultBrowserPromptManager::GetInstance()->CloseAllPrompts(
        DefaultBrowserPromptManager::CloseReason::kDismiss);
  }

  std::unique_ptr<DefaultBrowserModalHandler> CreateHandler() {
    mojo::PendingRemote<default_browser_modal::mojom::Page> page;
    std::ignore = page.InitWithNewPipeAndPassReceiver();
    return std::make_unique<TestDefaultBrowserModalHandler>(
        &web_ui_, std::move(page),
        handler_remote_.BindNewPipeAndPassReceiver());
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestWebUI web_ui_;
  mojo::Remote<default_browser_modal::mojom::PageHandler> handler_remote_;
};

TEST_F(DefaultBrowserModalHandlerTest, TryAgainIncrementsRetryCount) {
  base::HistogramTester histogram_tester;
  auto* prompt_manager = DefaultBrowserPromptManager::GetInstance();
  auto owned_surface_manager = std::make_unique<StickyTestSurfaceManager>();
  StickyTestSurfaceManager* surface_manager = owned_surface_manager.get();
  prompt_manager->SetPromptSurfaceManagerForTesting(
      std::move(owned_surface_manager));
  surface_manager->Show(/*can_pin_to_taskbar=*/false);
  surface_manager->HandleAccept();

  std::unique_ptr<DefaultBrowserModalHandler> handler = CreateHandler();
  handler->TryAgain();
  handler->TryAgain();

  prompt_manager->CloseAllPrompts(
      DefaultBrowserPromptManager::CloseReason::kDismiss);
  histogram_tester.ExpectUniqueSample(kStickyRetryCountHistogram, 2, 1);
}

TEST_F(DefaultBrowserModalHandlerTest, TryAgainWithoutSurfaceManagerIsNoOp) {
  base::HistogramTester histogram_tester;
  ASSERT_FALSE(
      DefaultBrowserPromptManager::GetInstance()->GetPromptSurfaceManager());

  std::unique_ptr<DefaultBrowserModalHandler> handler = CreateHandler();
  handler->TryAgain();

  histogram_tester.ExpectTotalCount(kStickyRetryCountHistogram, 0);
}

}  // namespace
