// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere_service.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service_factory.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class OmniboxEverywhereServiceTest : public testing::Test {
 public:
  OmniboxEverywhereServiceTest() {
    feature_list_.InitAndEnableFeature(omnibox::kOmniboxEverywhere);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(OmniboxEverywhereServiceTest, GetForProfile) {
  TestingProfile profile;
  OmniboxEverywhereService* service =
      OmniboxEverywhereServiceFactory::GetForProfile(&profile);
  ASSERT_TRUE(service);
  EXPECT_FALSE(service->IsPopupVisible());
}
