// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/browser/pdf_navigation_throttle.h"

#include "base/test/scoped_feature_list.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_renderer_host.h"
#include "pdf/pdf_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace pdf {

namespace {

class PdfNavigationThrottleTest : public content::RenderViewHostTestHarness {
 protected:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    navigation_handle_ = std::make_unique<content::MockNavigationHandle>(
        GURL("chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/"
             "00000000-0000-0000-0000-000000000000"),
        main_rfh());
  }

  base::test::ScopedFeatureList features_;
  std::unique_ptr<content::MockNavigationHandle> navigation_handle_;
};

class PdfNavigationThrottleUnseasonedDisabledTest
    : public PdfNavigationThrottleTest {
 protected:
  PdfNavigationThrottleUnseasonedDisabledTest() {
    features_.InitAndDisableFeature(chrome_pdf::features::kPdfUnseasoned);
  }
};

class PdfNavigationThrottleUnseasonedEnabledTest
    : public PdfNavigationThrottleTest {
 protected:
  PdfNavigationThrottleUnseasonedEnabledTest() {
    features_.InitAndEnableFeature(chrome_pdf::features::kPdfUnseasoned);
  }
};

}  // namespace

TEST_F(PdfNavigationThrottleUnseasonedDisabledTest, MaybeCreateThrottleFor) {
  EXPECT_FALSE(
      PdfNavigationThrottle::MaybeCreateThrottleFor(navigation_handle_.get()));
}

TEST_F(PdfNavigationThrottleUnseasonedEnabledTest, MaybeCreateThrottleFor) {
  EXPECT_TRUE(
      PdfNavigationThrottle::MaybeCreateThrottleFor(navigation_handle_.get()));
}

}  // namespace pdf
