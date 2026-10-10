// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/payments_churned_users_ui_delegate_desktop.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/autofill/payments/payments_churned_users_bubble_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace autofill::payments {
namespace {

class MockPaymentsChurnedUsersBubbleController
    : public PaymentsChurnedUsersBubbleController {
 public:
  explicit MockPaymentsChurnedUsersBubbleController(
      tabs::TabInterface& tab_interface,
      content::WebContents* web_contents)
      : PaymentsChurnedUsersBubbleController(tab_interface, web_contents) {}
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
    ON_CALL(mock_tab_interface_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(tab_unowned_user_data_host_));

    bubble_controller_ =
        std::make_unique<MockPaymentsChurnedUsersBubbleController>(
            mock_tab_interface_, web_contents());

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

}  // namespace
}  // namespace autofill::payments
