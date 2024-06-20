// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_actions.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/autofill/address_bubbles_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/actions/actions.h"

namespace chrome {

class BrowserActionsBrowserTest : public InProcessBrowserTest {
 public:
  BrowserActionsBrowserTest() = default;

 protected:
  raw_ptr<content::WebContents> web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  base::test::ScopedFeatureList feature_list_{features::kToolbarPinning};
};

IN_PROC_BROWSER_TEST_F(BrowserActionsBrowserTest, ShowAddressesBubbleOrPage) {
  auto& action_manager = actions::ActionManager::GetForTesting();
  const GURL addresses_url = GURL("chrome://settings/addresses");

  ASSERT_NE(web_contents()->GetURL(), addresses_url);
  action_manager.FindAction(kActionShowAddressesBubbleOrPage)->InvokeAction();
  EXPECT_EQ(web_contents()->GetURL(), addresses_url);

  autofill::AddressBubblesController::CreateForWebContents(web_contents());
  auto* bubble_controller =
      autofill::AddressBubblesController::FromWebContents(web_contents());
  ASSERT_EQ(bubble_controller->GetBubbleView(), nullptr);
  autofill::AddressBubblesController::SetUpAndShowAddNewAddressBubble(
      web_contents(), base::DoNothing());
  ASSERT_NE(bubble_controller->GetBubbleView(), nullptr);
  action_manager.FindAction(kActionShowAddressesBubbleOrPage)->InvokeAction();
  EXPECT_EQ(bubble_controller->GetBubbleView(), nullptr);
}

}  // namespace chrome
