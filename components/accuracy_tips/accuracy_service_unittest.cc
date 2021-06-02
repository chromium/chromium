// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accuracy_tips/accuracy_service.h"
#include <memory>

#include "base/metrics/field_trial_params.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/accuracy_tips/accuracy_tip_status.h"
#include "components/accuracy_tips/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace accuracy_tips {

class MockAccuracyTipUI : public AccuracyTipUI {
 public:
  MOCK_METHOD3(ShowAccuracyTip,
               void(content::WebContents*,
                    AccuracyTipStatus,
                    base::OnceCallback<void(Interaction)>));
};

class AccuracyServiceTest : public ::testing::Test {
 protected:
  AccuracyServiceTest() = default;

  void SetUp() override {
    base::FieldTrialParams params;
    params[kSampleUrl.name] = "https://badurl.com";
    feature_list.InitAndEnableFeatureWithParameters(kAccuracyTipsFeature,
                                                    params);
    auto ui = std::make_unique<testing::StrictMock<MockAccuracyTipUI>>();
    ui_ = ui.get();
    service_ = std::make_unique<AccuracyService>(std::move(ui));
  }

  AccuracyService* service() { return service_.get(); }
  MockAccuracyTipUI* ui() { return ui_; }

 private:
  base::test::ScopedFeatureList feature_list;
  std::unique_ptr<AccuracyService> service_;
  MockAccuracyTipUI* ui_;
};

TEST_F(AccuracyServiceTest, CheckAccuracyStatusForRandomSite) {
  base::MockOnceCallback<void(AccuracyTipStatus)> callback;
  EXPECT_CALL(callback, Run(AccuracyTipStatus::kNone));
  service()->CheckAccuracyStatus(GURL("https://example.com"), callback.Get());
}

TEST_F(AccuracyServiceTest, CheckAccuracyStatusForSampleUrl) {
  base::MockOnceCallback<void(AccuracyTipStatus)> callback;
  EXPECT_CALL(callback, Run(AccuracyTipStatus::kMisinformation));
  service()->CheckAccuracyStatus(GURL("https://badurl.com"), callback.Get());
}

TEST_F(AccuracyServiceTest, ShowUI) {
  EXPECT_CALL(*ui(), ShowAccuracyTip(_, _, _));
  service()->MaybeShowAccuracyTip(nullptr);
}

}  // namespace accuracy_tips