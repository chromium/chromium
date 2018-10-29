// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_pedal.h"

#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/omnibox/browser/omnibox_pedal_provider.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "components/omnibox/browser/test_omnibox_edit_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class OmniboxPedalTest : public testing::Test {
 protected:
  OmniboxPedalTest()
      : omnibox_client_(new TestOmniboxClient),
        omnibox_edit_controller_(new TestOmniboxEditController) {}

  TestToolbarModel* toolbar() {
    return omnibox_edit_controller_->GetToolbarModel();
  }

  base::MessageLoop message_loop_;
  std::unique_ptr<TestOmniboxClient> omnibox_client_;
  std::unique_ptr<TestOmniboxEditController> omnibox_edit_controller_;
};

TEST_F(OmniboxPedalTest, PedalExecutes) {
  OmniboxPedalProvider provider;
  base::TimeTicks match_selection_timestamp;
  OmniboxPedal::ExecutionContext context(
      *omnibox_client_, *omnibox_edit_controller_, match_selection_timestamp);
  {
    const base::string16 trigger = base::ASCIIToUTF16("clear history");
    const OmniboxPedal* pedal = provider.FindPedalMatch(trigger);
    EXPECT_NE(pedal, nullptr) << "Pedal not registered or not triggered.";
    EXPECT_TRUE(pedal->IsTriggerMatch(trigger));
    pedal->Execute(context);
    const GURL& url = omnibox_edit_controller_->destination_url();
    EXPECT_EQ(url, GURL("chrome://settings/clearBrowserData"));
  }
}
