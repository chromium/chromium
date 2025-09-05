// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_delegate.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/test/mock_callback.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace session_restore_infobar {

class SessionRestoreInfoBarDelegateTest : public testing::Test {
 public:
  SessionRestoreInfoBarDelegateTest() = default;
  SessionRestoreInfoBarDelegateTest(const SessionRestoreInfoBarDelegateTest&) =
      delete;
  SessionRestoreInfoBarDelegateTest& operator=(
      const SessionRestoreInfoBarDelegateTest&) = delete;

  SessionRestoreInfoBarDelegate* delegate() { return delegate_.get(); }
  base::MockRepeatingCallback<void()>& close_cb() { return close_cb_; }

  void CreateDelegate() {
    delegate_ = std::make_unique<SessionRestoreInfoBarDelegate>(
        close_cb_.Get(),
        SessionRestoreInfoBarDelegate::InfobarMessageType::kNone);
  }

 private:
  base::MockRepeatingCallback<void()> close_cb_;
  std::unique_ptr<SessionRestoreInfoBarDelegate> delegate_;
};

TEST_F(SessionRestoreInfoBarDelegateTest, GetIdentifier) {
  CreateDelegate();
  EXPECT_EQ(delegate()->GetIdentifier(),
            infobars::InfoBarDelegate::SESSION_RESTORE_INFOBAR_DELEGATE);
}

TEST_F(SessionRestoreInfoBarDelegateTest, GetVectorIcon) {
  CreateDelegate();
  EXPECT_EQ(&delegate()->GetVectorIcon(), &vector_icons::kProductRefreshIcon);
}

TEST_F(SessionRestoreInfoBarDelegateTest, GetButtons) {
  CreateDelegate();
  EXPECT_EQ(delegate()->GetButtons(),
            SessionRestoreInfoBarDelegate::BUTTON_NONE);
}

TEST_F(SessionRestoreInfoBarDelegateTest, ShouldShowLinkBeforeButton) {
  CreateDelegate();
  EXPECT_TRUE(delegate()->ShouldShowLinkBeforeButton());
}

}  // namespace session_restore_infobar
