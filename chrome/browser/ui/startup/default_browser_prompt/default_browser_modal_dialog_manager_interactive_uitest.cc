// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/default_browser/default_browser_controller.h"
#include "chrome/browser/default_browser/default_browser_features.h"
#include "chrome/browser/default_browser/test_support/fake_default_browser_setter.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_modal_dialog_manager.h"
#include "chrome/browser/ui/webui/default_browser/default_browser_modal_dialog_delegate.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"

class DefaultBrowserModalDialogManagerInteractiveTest
    : public InteractiveBrowserTest {
 protected:
  DefaultBrowserModalDialogManagerInteractiveTest() = default;
  ~DefaultBrowserModalDialogManagerInteractiveTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        default_browser::kDefaultBrowserPromptSurfaces,
        {{"prompt_surface", "modal_dialog_without_settings_illustration"}});
    InteractiveBrowserTest::SetUp();
  }

  void TearDownOnMainThread() override {
    manager_.reset();
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  void ShowDialogManager(bool use_settings_illustration) {
    manager_ =
        std::make_unique<default_browser::DefaultBrowserModalDialogManager>(
            use_settings_illustration);
    manager_->Show(/*can_pin_to_taskbar=*/false);
  }

  void CloseDialogs() { manager_->CloseAll(); }

  void DismissDialogs() {
    manager_->HandleDismiss();
    manager_->CloseAll();
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

  std::unique_ptr<default_browser::DefaultBrowserModalDialogManager> manager_;
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(DefaultBrowserModalDialogManagerInteractiveTest,
                       ShowAndIgnoreWithoutIllustration) {
  RunTestSequence(
      Do([this]() { ShowDialogManager(/*use_settings_illustration=*/false); }),
      InAnyContext(WaitForShow(default_browser::DefaultBrowserModalDialog::
                                   kDefaultBrowserModalDialogId)),
      VerifyHistogram("DefaultBrowser.ModalDialogWithoutSettingsIllustration."
                      "ShellIntegration.Shown",
                      1, 1),
      Do([this]() { CloseDialogs(); }),
      InAnyContext(WaitForHide(default_browser::DefaultBrowserModalDialog::
                                   kDefaultBrowserModalDialogId)),
      Do([this]() { manager_.reset(); }),
      VerifyHistogram(
          "DefaultBrowser.ModalDialogWithoutSettingsIllustration."
          "ShellIntegration.Interaction",
          static_cast<int>(
              default_browser::DefaultBrowserInteractionType::kIgnored),
          1));
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserModalDialogManagerInteractiveTest,
                       ShowAndDismissWithoutIllustration) {
  RunTestSequence(
      Do([this]() { ShowDialogManager(/*use_settings_illustration=*/false); }),
      InAnyContext(WaitForShow(default_browser::DefaultBrowserModalDialog::
                                   kDefaultBrowserModalDialogId)),
      VerifyHistogram("DefaultBrowser.ModalDialogWithoutSettingsIllustration."
                      "ShellIntegration.Shown",
                      1, 1),
      Do([this]() { DismissDialogs(); }),
      InAnyContext(WaitForHide(default_browser::DefaultBrowserModalDialog::
                                   kDefaultBrowserModalDialogId)),
      VerifyHistogram(
          "DefaultBrowser.ModalDialogWithoutSettingsIllustration."
          "ShellIntegration.Interaction",
          static_cast<int>(
              default_browser::DefaultBrowserInteractionType::kDismissed),
          1));
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserModalDialogManagerInteractiveTest,
                       ShowAndIgnoreWithIllustration) {
  RunTestSequence(
      Do([this]() { ShowDialogManager(/*use_settings_illustration=*/true); }),
      InAnyContext(WaitForShow(default_browser::DefaultBrowserModalDialog::
                                   kDefaultBrowserModalDialogId)),
      VerifyHistogram("DefaultBrowser.ModalDialogWithSettingsIllustration."
                      "ShellIntegration.Shown",
                      1, 1),
      Do([this]() { CloseDialogs(); }),
      InAnyContext(WaitForHide(default_browser::DefaultBrowserModalDialog::
                                   kDefaultBrowserModalDialogId)),
      Do([this]() { manager_.reset(); }),
      VerifyHistogram(
          "DefaultBrowser.ModalDialogWithSettingsIllustration."
          "ShellIntegration.Interaction",
          static_cast<int>(
              default_browser::DefaultBrowserInteractionType::kIgnored),
          1));
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserModalDialogManagerInteractiveTest,
                       ShowAndDismissWithIllustration) {
  RunTestSequence(
      Do([this]() { ShowDialogManager(/*use_settings_illustration=*/true); }),
      InAnyContext(WaitForShow(default_browser::DefaultBrowserModalDialog::
                                   kDefaultBrowserModalDialogId)),
      VerifyHistogram("DefaultBrowser.ModalDialogWithSettingsIllustration."
                      "ShellIntegration.Shown",
                      1, 1),
      Do([this]() { DismissDialogs(); }),
      InAnyContext(WaitForHide(default_browser::DefaultBrowserModalDialog::
                                   kDefaultBrowserModalDialogId)),
      VerifyHistogram(
          "DefaultBrowser.ModalDialogWithSettingsIllustration."
          "ShellIntegration.Interaction",
          static_cast<int>(
              default_browser::DefaultBrowserInteractionType::kDismissed),
          1));
}
