// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_assistant/password_change/assistant_stopped_bubble_coordinator_impl.h"

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

using CloseReason = AssistantStoppedBubbleCoordinatorImpl::CloseReason;

namespace {

constexpr char kUmaKeyAssistantStoppedBubbleCloseReason[] =
    "PasswordManager.AutomaticChange.AssistantStoppedBubbleCloseReason";

constexpr char kUrl[] = "https://www.example.com";
constexpr char kUsername[] = "anna";

}  // namespace

class AssistantStoppedBubbleCoordinatorImplTest : public DialogBrowserTest {
 public:
  AssistantStoppedBubbleCoordinatorImplTest() = default;

  AssistantStoppedBubbleCoordinatorImplTest(
      const AssistantStoppedBubbleCoordinatorImplTest&) = delete;
  AssistantStoppedBubbleCoordinatorImplTest& operator=(
      const AssistantStoppedBubbleCoordinatorImplTest&) = delete;

  ~AssistantStoppedBubbleCoordinatorImplTest() override = default;

  void ShowUi(const std::string& name) override {
    assistant_stopped_bubble_ =
        std::make_unique<AssistantStoppedBubbleCoordinatorImpl>(
            browser()->tab_strip_model()->GetActiveWebContents(), GURL(kUrl),
            kUsername);
    assistant_stopped_bubble_->Show();
  }

  AssistantStoppedBubbleCoordinatorImpl* assistant_stopped_bubble() {
    return assistant_stopped_bubble_.get();
  }

  // Simulates the destruction of the bubble coordinator that normally happens
  // on tab or browser close.
  void DestroyBubbleCoordinator() { assistant_stopped_bubble_.reset(); }

 private:
  std::unique_ptr<AssistantStoppedBubbleCoordinatorImpl>
      assistant_stopped_bubble_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(AssistantStoppedBubbleCoordinatorImplTest,
                       InvokeUi_AssistantStoppedBubbleCoordinatorImpl) {
  // No bubble until Show() is called.
  ASSERT_FALSE(VerifyUi());

  // Bubble is rendered on show.
  ShowAndVerifyUi();

  // Hides the bubble and asserts ui.
  assistant_stopped_bubble()->Hide();
  ASSERT_FALSE(VerifyUi());
}

IN_PROC_BROWSER_TEST_F(AssistantStoppedBubbleCoordinatorImplTest,
                       RecordsMetricOnRestartLinkClick) {
  base::HistogramTester histogram_tester;
  ShowUi("");

  assistant_stopped_bubble()->RestartLinkClicked(nullptr);
  DestroyBubbleCoordinator();
  histogram_tester.ExpectUniqueSample(kUmaKeyAssistantStoppedBubbleCloseReason,
                                      CloseReason::kRestartLinkClicked, 1u);
}

IN_PROC_BROWSER_TEST_F(AssistantStoppedBubbleCoordinatorImplTest,
                       RecordsMetricOnExplicitClose) {
  base::HistogramTester histogram_tester;
  ShowUi("");

  assistant_stopped_bubble()->Close();
  DestroyBubbleCoordinator();
  histogram_tester.ExpectUniqueSample(kUmaKeyAssistantStoppedBubbleCloseReason,
                                      CloseReason::kBubbleClosedExplicitly, 1u);
}

IN_PROC_BROWSER_TEST_F(AssistantStoppedBubbleCoordinatorImplTest,
                       RecordsMetricOnImplicitClose) {
  base::HistogramTester histogram_tester;
  ShowUi("");

  DestroyBubbleCoordinator();
  histogram_tester.ExpectUniqueSample(kUmaKeyAssistantStoppedBubbleCloseReason,
                                      CloseReason::kBubbleClosedImplicitly, 1u);
}
