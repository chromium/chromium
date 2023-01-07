// Copyright 2022 The Chromium Authors
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
#include "chrome/test/base/interactive_test_utils.h"
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

void ClickOnViewInBubble(views::BubbleDialogDelegate* bubble_delegate,
                         views::View* button) {
  // Reset the timer to make sure that test click isn't discarded as possibly
  // unintended.
  bubble_delegate->ResetViewShownTimeStampForTesting();
  ui_test_utils::ClickOnView(button);
}

}  // namespace

class PrivacySandboxNoticeBubbleInteractiveUiTest
    : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    mock_service_ = static_cast<MockPrivacySandboxService*>(
        PrivacySandboxServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            browser()->profile(),
            base::BindRepeating(&BuildMockPrivacySandboxService)));
  }

  MockPrivacySandboxService* mock_service() { return mock_service_; }

  ui::TrackedElement* GetElement(ui::ElementIdentifier id) {
    return ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
        id, browser()->window()->GetElementContext());
  }

 private:
  raw_ptr<MockPrivacySandboxService> mock_service_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxNoticeBubbleInteractiveUiTest,
                       // TODO(crbug.com/1356871): Re-enable this test
                       DISABLED_AcknowledgeNotice) {
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
  views::test::WidgetDestroyedWaiter observer(bubble);
  ClickOnViewInBubble(bubble_delegate, bubble_delegate->GetOkButton());
  observer.Wait();
  testing::Mock::VerifyAndClearExpectations(mock_service());
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxNoticeBubbleInteractiveUiTest,
                       // TODO(crbug.com/1356871): Re-enable this test
                       DISABLED_OpenSettingsNotice) {
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
  views::test::WidgetDestroyedWaiter observer(bubble);
  ClickOnViewInBubble(bubble_delegate, bubble_delegate->GetExtraView());
  observer.Wait();
  testing::Mock::VerifyAndClearExpectations(mock_service());
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxNoticeBubbleInteractiveUiTest,
                       // TODO(crbug.com/1356871): Re-enable this test
                       DISABLED_OpenLearnMoreNotice) {
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

  auto* bubble = ShowBubble(browser());
  auto* bubble_delegate = bubble->widget_delegate()->AsBubbleDialogDelegate();
  bubble->GetRootView()->RequestFocus();
  auto* view = GetElement(kPrivacySandboxLearnMoreTextForTesting)
                   ->AsA<views::TrackedElementViews>()
                   ->view();
  views::test::WidgetDestroyedWaiter observer(bubble);
  ClickOnViewInBubble(
      bubble_delegate,
      static_cast<views::StyledLabel*>(view)->GetFirstLinkForTesting());
  observer.Wait();
  testing::Mock::VerifyAndClearExpectations(mock_service());
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxNoticeBubbleInteractiveUiTest,
                       // TODO(crbug.com/1356871): Re-enable this test
                       DISABLED_EscapeClosesNotice) {
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
  views::test::WidgetDestroyedWaiter observer(bubble);
  bubble->CloseWithReason(views::Widget::ClosedReason::kEscKeyPressed);
  observer.Wait();
  testing::Mock::VerifyAndClearExpectations(mock_service());
}
