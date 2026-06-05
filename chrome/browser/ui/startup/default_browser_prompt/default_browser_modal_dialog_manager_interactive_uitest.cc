// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/default_browser/default_browser_controller.h"
#include "chrome/browser/default_browser/default_browser_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"
#include "chrome/browser/ui/webui/default_browser/default_browser_modal_dialog_delegate.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"

class DefaultBrowserModalDialogManagerInteractiveTest
    : public InteractiveBrowserTest,
      public testing::WithParamInterface<bool> {
 protected:
  DefaultBrowserModalDialogManagerInteractiveTest() = default;
  ~DefaultBrowserModalDialogManagerInteractiveTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        default_browser::kDefaultBrowserPromptSurfaces,
        {{"prompt_surface",
          GetParam() ? "modal_dialog_with_settings_illustration"
                     : "modal_dialog_without_settings_illustration"}});
    InteractiveBrowserTest::SetUp();
  }

  void TearDownOnMainThread() override {
    DefaultBrowserPromptManager::GetInstance()->CloseAllPrompts(
        DefaultBrowserPromptManager::CloseReason::kDismiss);
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  void ShowDialog() {
    DefaultBrowserPromptManager::GetInstance()->ShowPrompts(
        /*can_pin_to_taskbar=*/false);
  }

  void CloseDialogs() {
    DefaultBrowserPromptManager::GetInstance()->CloseAllPrompts(
        DefaultBrowserPromptManager::CloseReason::kDismiss);
  }

  void DismissDialogs() {
    auto* prompt_manager = DefaultBrowserPromptManager::GetInstance();
    if (auto* surface_manager = prompt_manager->GetPromptSurfaceManager()) {
      surface_manager->HandleDismiss();
    }
    prompt_manager->CloseAllPrompts(
        DefaultBrowserPromptManager::CloseReason::kDismiss);
  }

  MultiStep VerifyHistogram(const std::string& histogram_name,
                            int bucket,
                            int count) {
    MultiStep steps;

    steps += Do([this, histogram_name, bucket, count]() {
      histogram_tester_.ExpectBucketCount(histogram_name, bucket, count);
    });
    return steps;
  }

  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(DefaultBrowserModalDialogManagerInteractiveTest,
                       ShowAndIgnore) {
  RunTestSequence(
      Do([this]() { ShowDialog(); }),
      InAnyContext(WaitForShow(default_browser::kDefaultBrowserModalDialogId)),
      VerifyHistogram(
          GetParam() ? "DefaultBrowser.ModalDialogWithSettingsIllustration."
                       "ShellIntegration.Shown"
                     : "DefaultBrowser.ModalDialogWithoutSettingsIllustration."
                       "ShellIntegration.Shown",
          1, 1),
      Do([this]() { CloseDialogs(); }),
      InAnyContext(WaitForHide(default_browser::kDefaultBrowserModalDialogId)),
      VerifyHistogram(
          GetParam() ? "DefaultBrowser.ModalDialogWithSettingsIllustration."
                       "ShellIntegration.Interaction"
                     : "DefaultBrowser.ModalDialogWithoutSettingsIllustration."
                       "ShellIntegration.Interaction",
          static_cast<int>(
              default_browser::DefaultBrowserInteractionType::kIgnored),
          1));
}

IN_PROC_BROWSER_TEST_P(DefaultBrowserModalDialogManagerInteractiveTest,
                       ShowAndDismiss) {
  RunTestSequence(
      Do([this]() { ShowDialog(); }),
      InAnyContext(WaitForShow(default_browser::kDefaultBrowserModalDialogId)),
      VerifyHistogram(
          GetParam() ? "DefaultBrowser.ModalDialogWithSettingsIllustration."
                       "ShellIntegration.Shown"
                     : "DefaultBrowser.ModalDialogWithoutSettingsIllustration."
                       "ShellIntegration.Shown",
          1, 1),
      Do([this]() { DismissDialogs(); }),
      InAnyContext(WaitForHide(default_browser::kDefaultBrowserModalDialogId)),
      VerifyHistogram(
          GetParam() ? "DefaultBrowser.ModalDialogWithSettingsIllustration."
                       "ShellIntegration.Interaction"
                     : "DefaultBrowser.ModalDialogWithoutSettingsIllustration."
                       "ShellIntegration.Interaction",
          static_cast<int>(
              default_browser::DefaultBrowserInteractionType::kDismissed),
          1));
}

IN_PROC_BROWSER_TEST_P(DefaultBrowserModalDialogManagerInteractiveTest,
                       CloseWidgetViaEscAndResize) {
  ui::Accelerator esc_accelerator(ui::VKEY_ESCAPE, ui::EF_NONE);
  RunTestSequence(
      Do([this]() { ShowDialog(); }),
      InAnyContext(WaitForShow(default_browser::kDefaultBrowserModalDialogId)),
      VerifyHistogram(
          GetParam() ? "DefaultBrowser.ModalDialogWithSettingsIllustration."
                       "ShellIntegration.Shown"
                     : "DefaultBrowser.ModalDialogWithoutSettingsIllustration."
                       "ShellIntegration.Shown",
          1, 1),
      SendAccelerator(default_browser::kDefaultBrowserModalDialogId,
                      esc_accelerator),
      InAnyContext(WaitForHide(default_browser::kDefaultBrowserModalDialogId)),
      Do([this]() {
        // Resize the parent browser window to trigger any layout/positioning
        // updates and verify that no crash occurs.
        browser()->window()->SetBounds(gfx::Rect(10, 10, 800, 600));
      }),
      VerifyHistogram(
          GetParam() ? "DefaultBrowser.ModalDialogWithSettingsIllustration."
                       "ShellIntegration.Interaction"
                     : "DefaultBrowser.ModalDialogWithoutSettingsIllustration."
                       "ShellIntegration.Interaction",
          static_cast<int>(
              default_browser::DefaultBrowserInteractionType::kDismissed),
          1));
}

INSTANTIATE_TEST_SUITE_P(All,
                         DefaultBrowserModalDialogManagerInteractiveTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "WithIllustration"
                                             : "WithoutIllustration";
                         });
