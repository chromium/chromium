// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/address_profiles/save_address_profile_bubble_controller.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class SaveAddressProfileBubbleControllerTest
    : public BrowserWithTestWindowTest {
 public:
  SaveAddressProfileBubbleControllerTest() = default;
  void SetUp() override {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        features::kAutofillAddressProfileSavePrompt);

    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("about:blank"));
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    SaveAddressProfileBubbleController::CreateForWebContents(web_contents);
  }
};

TEST_F(SaveAddressProfileBubbleControllerTest, NoCrash) {}

}  // namespace autofill
