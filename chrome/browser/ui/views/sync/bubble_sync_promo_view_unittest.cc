// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sync/bubble_sync_promo_view.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/sync/bubble_sync_promo_delegate.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/range/range.h"
#include "ui/views/controls/styled_label.h"

class BubbleSyncPromoViewTest : public ChromeViewsTestBase,
                                public BubbleSyncPromoDelegate {
 public:
  BubbleSyncPromoViewTest() {}

 protected:
  // BubbleSyncPromoDelegate:
  void OnEnableSync(const AccountInfo& account,
                    bool is_default_promo_account) override {
    // The bubble sync promo view does not allow the user to enable sync
    // for an existing account id.
    DCHECK(account.IsEmpty());
    ++on_enable_sync_count_;
  }

  // Number of times that OnSignInLinkClicked has been called.
  int on_enable_sync_count_ = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(BubbleSyncPromoViewTest);
};

TEST_F(BubbleSyncPromoViewTest, SignInLink) {
  std::unique_ptr<BubbleSyncPromoView> sync_promo;
  sync_promo = std::make_unique<BubbleSyncPromoView>(
      this, signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE,
      IDS_BOOKMARK_SYNC_PROMO_LINK, IDS_BOOKMARK_SYNC_PROMO_MESSAGE);

  // Simulate clicking the "Sign in" link.
  views::StyledLabel styled_label(base::ASCIIToUTF16("test"), nullptr);
  views::StyledLabelListener* listener = sync_promo.get();
  listener->StyledLabelLinkClicked(&styled_label, gfx::Range(), ui::EF_NONE);

  EXPECT_EQ(1, on_enable_sync_count_);
}
