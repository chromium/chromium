// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/companion/promo_handler.h"

#include "chrome/browser/ui/webui/side_panel/companion/companion.mojom.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace companion {

class PromoHandlerTest : public testing::Test {
 public:
  PromoHandlerTest() = default;
  ~PromoHandlerTest() override = default;

  void SetUp() override {
    promo_handler_ = std::make_unique<PromoHandler>(&pref_service_);
  }

 protected:
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<PromoHandler> promo_handler_;
};

TEST_F(PromoHandlerTest, OnPromoActionTest) {
  promo_handler_->OnPromoAction(PromoType::kMsbb, PromoAction::kAccepted);
  // TODO(shaktisahu): Verify result.
}

}  // namespace companion
