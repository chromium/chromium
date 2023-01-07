// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/save_upi_bubble_controller_impl.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class SaveUPIBubbleControllerImplTest : public DialogBrowserTest {
 public:
  SaveUPIBubbleControllerImplTest() = default;
  SaveUPIBubbleControllerImplTest(const SaveUPIBubbleControllerImplTest&) =
      delete;
  SaveUPIBubbleControllerImplTest& operator=(
      const SaveUPIBubbleControllerImplTest&) = delete;
  ~SaveUPIBubbleControllerImplTest() override = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Do lazy initialization of SaveUPIBubbleControllerImpl.
    SaveUPIBubbleControllerImpl::CreateForWebContents(web_contents);
    controller_ = SaveUPIBubbleControllerImpl::FromWebContents(web_contents);
    DCHECK(controller_);

    controller_->OfferUpiIdLocalSave("user@indianbank", base::DoNothing());
  }

 private:
  raw_ptr<SaveUPIBubbleControllerImpl, DanglingUntriaged> controller_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(SaveUPIBubbleControllerImplTest, InvokeUi) {
  ShowAndVerifyUi();
}

}  // namespace autofill
