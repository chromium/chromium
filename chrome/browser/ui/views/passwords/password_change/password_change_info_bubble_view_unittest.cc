// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_change/password_change_info_bubble_view.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "chrome/browser/password_manager/password_change_delegate_mock.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_test_base.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/test_event.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/test/button_test_api.h"
#include "url/gurl.h"

using testing::Return;
using testing::ReturnRef;

namespace {
const std::u16string kTestEmail = u"account@example.com";

std::unique_ptr<KeyedService> BuildTestSyncService(
    AccountInfo account_info,
    content::BrowserContext* context) {
  auto sync_service = std::make_unique<syncer::TestSyncService>();
  sync_service->SetSignedIn(signin::ConsentLevel::kSync, account_info);
  return sync_service;
}

std::u16string GetBodyText() {
  const std::u16string link = l10n_util::GetStringUTF16(
      IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SYNCED_TO_ACCOUNT);
  return l10n_util::GetStringFUTF16(
      IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_INFO_BUBBLE_DETAILS, link,
      kTestEmail);
}
}  // namespace

class PasswordChangeInfoBubbleViewTest : public PasswordBubbleViewTestBase {
 public:
  PasswordChangeInfoBubbleViewTest() = default;
  ~PasswordChangeInfoBubbleViewTest() override = default;

  void SetUp() override {
    PasswordBubbleViewTestBase::SetUp();
    password_change_delegate_ = std::make_unique<PasswordChangeDelegateMock>();
    ON_CALL(*password_change_delegate_, GetDisplayOrigin())
        .WillByDefault(Return(u"example.com"));
    ON_CALL(*model_delegate_mock(), GetPasswordChangeDelegate())
        .WillByDefault(Return(password_change_delegate_.get()));
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

  void CreateAndShowView(PasswordChangeDelegate::State state) {
    CreateAnchorViewAndShow();

    ON_CALL(*password_change_delegate_, GetCurrentState())
        .WillByDefault(Return(state));
    view_ =
        new PasswordChangeInfoBubbleView(web_contents(), anchor_view(), state);
    views::BubbleDialogDelegateView::CreateBubble(view_)->Show();
  }

  PasswordChangeDelegateMock* password_change_delegate() {
    return password_change_delegate_.get();
  }

  PasswordChangeInfoBubbleView* view() { return view_; }

 private:
  std::unique_ptr<PasswordChangeDelegateMock> password_change_delegate_;
  raw_ptr<PasswordChangeInfoBubbleView> view_;
};

TEST_F(PasswordChangeInfoBubbleViewTest, ChangingPasswordBubbleLayout) {
  CreateAndShowView(PasswordChangeDelegate::State::kChangingPassword);

  EXPECT_TRUE(view()->GetCancelButton());
  views::StyledLabel* bodyTextLabel =
      static_cast<views::StyledLabel*>(view()->GetViewByID(
          PasswordChangeInfoBubbleView::kChangingPasswordBodyText));
  EXPECT_EQ(bodyTextLabel->GetText(), GetBodyText());
  EXPECT_EQ(view()->GetWindowTitle(),
            l10n_util::GetStringUTF16(
                IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_INFO_BUBBLE_TITLE));
}

TEST_F(PasswordChangeInfoBubbleViewTest, CancelClosesTheBubbleAndCancelsFlow) {
  CreateAndShowView(PasswordChangeDelegate::State::kChangingPassword);

  EXPECT_CALL(*password_change_delegate(), Stop);
  EXPECT_CALL(*model_delegate_mock(), OnBubbleHidden);
  views::test::ButtonTestApi(view()->GetCancelButton())
      .NotifyClick(ui::test::TestEvent());
}
