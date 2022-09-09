// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_assistant/password_change/assistant_stopped_bubble_coordinator_impl.h"

#include <memory>
#include <string>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

namespace {

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

  // Hides the bubble and assert ui.
  assistant_stopped_bubble()->Hide();
  ASSERT_FALSE(VerifyUi());
}
