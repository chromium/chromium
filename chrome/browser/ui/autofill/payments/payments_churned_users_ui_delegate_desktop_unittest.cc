// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/payments_churned_users_ui_delegate_desktop.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/autofill/payments/payments_churned_users_bubble_controller.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/test_support/mock_page_action_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/ui/payments/payments_ui_closed_reasons.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace autofill::payments {
namespace {

class MockPaymentsChurnedUsersBubbleController
    : public PaymentsChurnedUsersBubbleController {
 public:
  explicit MockPaymentsChurnedUsersBubbleController(
      tabs::TabInterface* tab_interface)
      : PaymentsChurnedUsersBubbleController(tab_interface) {}
  ~MockPaymentsChurnedUsersBubbleController() override = default;

  MOCK_METHOD(void,
              Show,
              (base::OnceCallback<void(PaymentsUiClosedReason)> closed_callback,
               AccountInfo account_info),
              (override));
};

class PaymentsChurnedUsersUiDelegateDesktopTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PaymentsChurnedUsersUiDelegateDesktopTest() = default;
  ~PaymentsChurnedUsersUiDelegateDesktopTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("about:blank"));

    tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                         &mock_tab_interface_);
    ON_CALL(mock_tab_interface_, GetContents())
        .WillByDefault(testing::Return(web_contents()));
    ON_CALL(mock_tab_interface_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(tab_unowned_user_data_host_));

    bubble_controller_ =
        std::make_unique<MockPaymentsChurnedUsersBubbleController>(
            &mock_tab_interface_);

    client_ = std::make_unique<TestContentAutofillClient>(web_contents());
    delegate_ =
        std::make_unique<PaymentsChurnedUsersUiDelegateDesktop>(client_.get());
  }

  void TearDown() override {
    delegate_.reset();
    client_.reset();
    bubble_controller_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  MockPaymentsChurnedUsersBubbleController& bubble_controller() {
    return *bubble_controller_;
  }

  PaymentsChurnedUsersUiDelegateDesktop& delegate() { return *delegate_; }

  ui::UnownedUserDataHost& tab_unowned_user_data_host() {
    return tab_unowned_user_data_host_;
  }

 private:
  ui::UnownedUserDataHost tab_unowned_user_data_host_;
  tabs::MockTabInterface mock_tab_interface_;

  std::unique_ptr<MockPaymentsChurnedUsersBubbleController> bubble_controller_;
  std::unique_ptr<TestContentAutofillClient> client_;
  std::unique_ptr<PaymentsChurnedUsersUiDelegateDesktop> delegate_;
};

TEST_F(PaymentsChurnedUsersUiDelegateDesktopTest, ShowPaymentsChurnedUsersUI) {
  AccountInfo account_info =
      AccountInfo::Builder(GaiaId("test_id"), "test@example.com").Build();

  EXPECT_CALL(bubble_controller(),
              Show(testing::_, testing::Property(&AccountInfo::GetEmail,
                                                 "test@example.com")));

  delegate().ShowPaymentsChurnedUsersUI(account_info,
                                        /*closed_callback=*/base::DoNothing());
}

// Tests that OnBubbleDiscarded() hides the page action and runs the callback.
TEST_F(PaymentsChurnedUsersUiDelegateDesktopTest,
       OnBubbleDiscarded_HidesPageActionAndRunsCallback) {
  page_actions::MockPageActionController mock_page_action_controller;
  ui::ScopedUnownedUserData<page_actions::PageActionController>
      scoped_page_action_controller(tab_unowned_user_data_host(),
                                    mock_page_action_controller);

  AccountInfo account_info =
      AccountInfo::Builder(GaiaId("test_id"), "test@example.com").Build();
  base::test::TestFuture<PaymentsUiClosedReason> closed_future;

  EXPECT_CALL(mock_page_action_controller,
              Show(kActionShowPaymentsChurnedUsersBubble))
      .Times(2);
  bubble_controller().PaymentsChurnedUsersBubbleController::Show(
      closed_future.GetCallback(), account_info);
  ASSERT_FALSE(closed_future.IsReady());

  EXPECT_CALL(mock_page_action_controller,
              Hide(kActionShowPaymentsChurnedUsersBubble));
  bubble_controller().OnBubbleDiscarded();
  EXPECT_EQ(closed_future.Get(), PaymentsUiClosedReason::kNotInteracted);
}

}  // namespace
}  // namespace autofill::payments
