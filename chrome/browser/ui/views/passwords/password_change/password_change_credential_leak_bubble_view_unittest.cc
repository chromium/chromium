// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_change/password_change_credential_leak_bubble_view.h"

#include "chrome/browser/password_manager/password_change_delegate_mock.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/passwords/bubble_controllers/password_change/password_change_credential_leak_bubble_controller.h"
#include "chrome/browser/ui/passwords/passwords_leak_dialog_delegate_mock.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_test_base.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/test_event.h"
#include "ui/views/test/button_test_api.h"

using testing::Invoke;
using testing::Return;
using ClosedReason = views::Widget::ClosedReason;

namespace {
const std::u16string kTestEmail = u"account@example.com";

std::unique_ptr<KeyedService> BuildTestSyncService(
    AccountInfo account_info,
    content::BrowserContext* context) {
  auto sync_service = std::make_unique<syncer::TestSyncService>();
  sync_service->SetSignedIn(signin::ConsentLevel::kSync, account_info);
  return sync_service;
}
}  // namespace

class PasswordChangeCredentialLeakBubbleViewTest
    : public PasswordBubbleViewTestBase {
 public:
  PasswordChangeCredentialLeakBubbleViewTest() = default;
  ~PasswordChangeCredentialLeakBubbleViewTest() override = default;

  void SetUp() override {
    PasswordBubbleViewTestBase::SetUp();
    password_change_delegate_ = std::make_unique<PasswordChangeDelegateMock>();
    ON_CALL(*password_change_delegate_, GetDisplayOrigin())
        .WillByDefault(Return(u"example.com"));
    ON_CALL(*model_delegate_mock(), GetPasswordChangeDelegate())
        .WillByDefault(Return(password_change_delegate_.get()));
    ON_CALL(*model_delegate_mock(), GetPasswordsLeakDialogDelegate())
        .WillByDefault(Return(&passwords_leak_dialog_delegate_));
    AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
        base::UTF16ToUTF8(kTestEmail), signin::ConsentLevel::kSignin);
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindRepeating(&BuildTestSyncService, std::move(account_info)));
  }

  void TearDown() override {
    view_->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kCloseButtonClicked);
    view_ = nullptr;
    PasswordBubbleViewTestBase::TearDown();
  }

  void CreateAndShowView() {
    CreateAnchorViewAndShow();

    view_ = new PasswordChangeCredentialLeakBubbleView(web_contents(),
                                                       anchor_view());
    views::BubbleDialogDelegateView::CreateBubble(view_)->Show();
  }

  PasswordChangeCredentialLeakBubbleView* view() { return view_; }

  PasswordChangeDelegateMock* password_change_delegate() {
    return password_change_delegate_.get();
  }

  PasswordsLeakDialogDelegateMock* passwords_leak_dialog_delegate() {
    return &passwords_leak_dialog_delegate_;
  }

 private:
  raw_ptr<PasswordChangeCredentialLeakBubbleView> view_;
  std::unique_ptr<PasswordChangeDelegateMock> password_change_delegate_;
  PasswordsLeakDialogDelegateMock passwords_leak_dialog_delegate_;
};

TEST_F(PasswordChangeCredentialLeakBubbleViewTest, ChangePasswordIsTriggered) {
  CreateAndShowView();

  EXPECT_CALL(*password_change_delegate(), StartPasswordChangeFlow);
  EXPECT_CALL(*model_delegate_mock(), OnBubbleHidden);
  views::test::ButtonTestApi(view()->GetOkButton())
      .NotifyClick(ui::test::TestEvent());
}

TEST_F(PasswordChangeCredentialLeakBubbleViewTest,
       PasswordChangeLinkIsTriggered) {
  CreateAndShowView();

  auto* controller = static_cast<PasswordChangeCredentialLeakBubbleController*>(
      static_cast<PasswordBubbleViewBase*>(view())->GetController());
  EXPECT_CALL(*model_delegate_mock(), NavigateToPasswordChangeSettings);
  controller->NavigateToPasswordChangeSettings();
}

TEST_F(PasswordChangeCredentialLeakBubbleViewTest,
       OnLeakDialogHiddenIsTriggeredOnClose) {
  CreateAndShowView();

  EXPECT_CALL(*passwords_leak_dialog_delegate(), OnLeakDialogHidden);
  view()->GetWidget()->CloseWithReason(ClosedReason::kCloseButtonClicked);
}
