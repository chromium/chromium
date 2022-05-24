// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/privacy_sandbox/mock_privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/privacy_sandbox/privacy_sandbox_prompt.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/privacy_sandbox/privacy_sandbox_notice_bubble.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

namespace {

views::Widget* ShowBubble(Browser* browser) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       kPrivacySandboxNoticeBubbleName);
  ShowPrivacySandboxPrompt(browser, PrivacySandboxService::PromptType::kNotice);
  return waiter.WaitIfNeededAndGet();
}

void ClickButton(views::BubbleDialogDelegate* bubble_delegate,
                 views::View* button) {
  // Reset the timer to make sure that test click isn't discarded as possibly
  // unintended.
  bubble_delegate->ResetViewShownTimeStampForTesting();
  gfx::Point center(button->width() / 2, button->height() / 2);
  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, center, center,
                             ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  button->OnMousePressed(event);
  button->OnMouseReleased(event);
}

}  // namespace

class PrivacySandboxNoticeBubbleBrowserTest : public DialogBrowserTest {
 public:
  void SetUpOnMainThread() override {
    mock_service_ = static_cast<MockPrivacySandboxService*>(
        PrivacySandboxServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            browser()->profile(),
            base::BindRepeating(&BuildMockPrivacySandboxService)));
  }

  void SetUp() override {
    const base::FieldTrialParams params = {
        {privacy_sandbox::kPrivacySandboxSettings3NewNotice.name, "true"}};
    feature_list_.InitWithFeaturesAndParameters(
        {{privacy_sandbox::kPrivacySandboxSettings3, params}}, {});
    InProcessBrowserTest::SetUp();
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override { ShowBubble(browser()); }

  MockPrivacySandboxService* mock_service() { return mock_service_; }

  void VerifyBubbleWasClosed(views::Widget* bubble) {
    EXPECT_TRUE(bubble->IsClosed());

    // While IsClosed updated immediately, the widget will only actually close,
    // and thus inform the service asynchronously so must be waited for.
    base::RunLoop().RunUntilIdle();

    // Shutting down the browser test will naturally shut the bubble, verify
    // expectations before that happens.
    testing::Mock::VerifyAndClearExpectations(mock_service());
  }

  ui::TrackedElement* GetElement(ui::ElementIdentifier id) {
    return ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
        id, browser()->window()->GetElementContext());
  }

 private:
  raw_ptr<MockPrivacySandboxService> mock_service_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxNoticeBubbleBrowserTest,
                       InvokeUi_NoticeBubble) {
  EXPECT_CALL(
      *mock_service(),
      PromptActionOccurred(PrivacySandboxService::PromptAction::kNoticeShown));
  EXPECT_CALL(
      *mock_service(),
      PromptActionOccurred(
          PrivacySandboxService::PromptAction::kNoticeClosedNoInteraction));
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxNoticeBubbleBrowserTest,
                       EscapeClosesNotice) {
  // Check that when the escape key is pressed, the notice bubble is closed.
  EXPECT_CALL(
      *mock_service(),
      PromptActionOccurred(PrivacySandboxService::PromptAction::kNoticeShown));
  EXPECT_CALL(*mock_service(),
              PromptActionOccurred(
                  PrivacySandboxService::PromptAction::kNoticeDismiss));
  EXPECT_CALL(
      *mock_service(),
      PromptActionOccurred(
          PrivacySandboxService::PromptAction::kNoticeClosedNoInteraction))
      .Times(0);
  auto* bubble = ShowBubble(browser());
  bubble->CloseWithReason(views::Widget::ClosedReason::kEscKeyPressed);
  VerifyBubbleWasClosed(bubble);
}

// TODO(crbug.com/1321587): Figure out what to do with the bubble when the app
// menu is shown. Update the test to test that.
IN_PROC_BROWSER_TEST_F(PrivacySandboxNoticeBubbleBrowserTest,
                       DISABLED_AppMenuClosesNotice) {
  // Check that when the app menu is opened, the notice bubble is closed.
  EXPECT_CALL(
      *mock_service(),
      PromptActionOccurred(PrivacySandboxService::PromptAction::kNoticeShown));
  EXPECT_CALL(*mock_service(),
              PromptActionOccurred(
                  PrivacySandboxService::PromptAction::kNoticeDismiss));
  EXPECT_CALL(
      *mock_service(),
      PromptActionOccurred(
          PrivacySandboxService::PromptAction::kNoticeClosedNoInteraction))
      .Times(0);
  auto* bubble = ShowBubble(browser());
  chrome::ShowAppMenu(browser());
  VerifyBubbleWasClosed(bubble);
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxNoticeBubbleBrowserTest,
                       AcknowledgeNotice) {
  EXPECT_CALL(
      *mock_service(),
      PromptActionOccurred(PrivacySandboxService::PromptAction::kNoticeShown));
  EXPECT_CALL(*mock_service(),
              PromptActionOccurred(
                  PrivacySandboxService::PromptAction::kNoticeAcknowledge));
  EXPECT_CALL(
      *mock_service(),
      PromptActionOccurred(
          PrivacySandboxService::PromptAction::kNoticeClosedNoInteraction))
      .Times(0);

  auto* bubble = ShowBubble(browser());
  auto* bubble_delegate = bubble->widget_delegate()->AsBubbleDialogDelegate();
  ClickButton(bubble_delegate, bubble_delegate->GetOkButton());
  VerifyBubbleWasClosed(bubble);
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxNoticeBubbleBrowserTest,
                       OpenSettingsNotice) {
  EXPECT_CALL(
      *mock_service(),
      PromptActionOccurred(PrivacySandboxService::PromptAction::kNoticeShown));
  EXPECT_CALL(*mock_service(),
              PromptActionOccurred(
                  PrivacySandboxService::PromptAction::kNoticeOpenSettings));
  EXPECT_CALL(
      *mock_service(),
      PromptActionOccurred(
          PrivacySandboxService::PromptAction::kNoticeClosedNoInteraction))
      .Times(0);

  auto* bubble = ShowBubble(browser());
  auto* bubble_delegate = bubble->widget_delegate()->AsBubbleDialogDelegate();
  ClickButton(bubble_delegate, bubble_delegate->GetExtraView());
  // TODO(crbug.com/1321587): Bubble should be closed. Figure out why opening
  // settings page doesn't take over the focus (only in tests).
}

// TODO(crbug.com/1321587, crbug.com/1327190): Update how the link is accessed
// after the API for DialogModel is added.
IN_PROC_BROWSER_TEST_F(PrivacySandboxNoticeBubbleBrowserTest,
                       OpenLearnMoreNotice) {
  EXPECT_CALL(
      *mock_service(),
      PromptActionOccurred(PrivacySandboxService::PromptAction::kNoticeShown));
  EXPECT_CALL(*mock_service(),
              PromptActionOccurred(
                  PrivacySandboxService::PromptAction::kNoticeLearnMore));
  EXPECT_CALL(
      *mock_service(),
      PromptActionOccurred(
          PrivacySandboxService::PromptAction::kNoticeClosedNoInteraction))
      .Times(0);

  ShowBubble(browser());
  auto* view = GetElement(kPrivacySandboxLearnMoreTextForTesting)
                   ->AsA<views::TrackedElementViews>()
                   ->view();
  static_cast<views::StyledLabel*>(view)->ClickLinkForTesting();
  // TODO(crbug.com/1321587): Bubble should be closed. Figure out why opening
  // settings page doesn't take over the focus (only in tests).
}
