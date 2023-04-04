// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/companion/promo_handler.h"

#include "chrome/browser/ui/webui/side_panel/companion/companion.mojom.h"
#include "chrome/browser/ui/webui/side_panel/companion/constants.h"
#include "chrome/browser/ui/webui/side_panel/companion/msbb_delegate.h"
#include "chrome/browser/ui/webui/side_panel/companion/signin_delegate.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace companion {
namespace {

class MockMsbbDelegate : public MsbbDelegate {
 public:
  MOCK_METHOD1(EnableMsbb, void(bool));
  MOCK_METHOD0(IsMsbbEnabled, bool());
};

class MockSigninDelegate : public SigninDelegate {
 public:
  MOCK_METHOD0(AllowedSignin, bool());
  MOCK_METHOD0(StartSigninFlow, void());
};

}  // namespace

class PromoHandlerTest : public testing::Test {
 public:
  PromoHandlerTest() = default;
  ~PromoHandlerTest() override = default;

  void SetUp() override {
    PromoHandler::RegisterProfilePrefs(pref_service_.registry());
    promo_handler_ = std::make_unique<PromoHandler>(
        &pref_service_, &signin_delegate_, &msbb_delegate_);
  }

 protected:
  TestingPrefServiceSimple pref_service_;
  MockSigninDelegate signin_delegate_;
  MockMsbbDelegate msbb_delegate_;
  std::unique_ptr<PromoHandler> promo_handler_;
};

TEST_F(PromoHandlerTest, MsbbPromo) {
  promo_handler_->OnPromoAction(PromoType::kMsbb, PromoAction::kRejected);
  EXPECT_EQ(1, pref_service_.GetInteger(kMsbbPromoDeclinedCountPref));

  EXPECT_CALL(msbb_delegate_, EnableMsbb(true)).Times(1);
  promo_handler_->OnPromoAction(PromoType::kMsbb, PromoAction::kAccepted);
}

TEST_F(PromoHandlerTest, SigninPromo) {
  promo_handler_->OnPromoAction(PromoType::kSignin, PromoAction::kRejected);
  EXPECT_EQ(1, pref_service_.GetInteger(kSigninPromoDeclinedCountPref));

  EXPECT_CALL(signin_delegate_, StartSigninFlow()).Times(1);
  promo_handler_->OnPromoAction(PromoType::kSignin, PromoAction::kAccepted);
}

TEST_F(PromoHandlerTest, LabsPromo) {
  promo_handler_->OnPromoAction(PromoType::kLabs, PromoAction::kRejected);
  EXPECT_EQ(1, pref_service_.GetInteger(kLabsPromoDeclinedCountPref));

  promo_handler_->OnPromoAction(PromoType::kLabs, PromoAction::kAccepted);
  // TODO(b/272954072): Add test.
}

}  // namespace companion
