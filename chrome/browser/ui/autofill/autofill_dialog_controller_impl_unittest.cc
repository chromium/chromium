// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_dialog_controller_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "chrome/browser/ui/autofill/mock_autofill_dialog_view.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {

using ::testing::NiceMock;

class AutofillDialogControllerImplTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    mock_view_ = std::make_unique<NiceMock<MockAutofillDialogView>>();
    mock_view_ptr_ = mock_view_.get();
    controller_ =
        std::make_unique<AutofillDialogControllerImpl>(web_contents());
    controller_->SetViewFactoryForTest(
        base::BindRepeating(&AutofillDialogControllerImplTest::PrepareMockView,
                            base::Unretained(this)));
  }

 protected:
  std::unique_ptr<AutofillDialogView> PrepareMockView() {
    return std::move(mock_view_);
  }

  std::unique_ptr<AutofillDialogControllerImpl> controller_;
  std::unique_ptr<NiceMock<MockAutofillDialogView>> mock_view_;
  raw_ptr<NiceMock<MockAutofillDialogView>> mock_view_ptr_ = nullptr;
};

// Test that the dialog is shown.
TEST_F(AutofillDialogControllerImplTest, ShowDialog) {
  EXPECT_CALL(*mock_view_ptr_, Show());
  controller_->Show(u"Title", u"Description", u"Button", base::DoNothing());

  EXPECT_EQ(u"Title", controller_->GetTitleText());
  EXPECT_EQ(u"Description", controller_->GetDescriptionText());
  EXPECT_EQ(u"Button", controller_->GetButtonText());
}

// Test that only one dialog is shown at a time.
TEST_F(AutofillDialogControllerImplTest, ShowDialogTwice) {
  EXPECT_CALL(*mock_view_ptr_, Show());
  controller_->Show(u"Title", u"Description", u"Button", base::DoNothing());
  controller_->Show(u"Title", u"Description", u"Button", base::DoNothing());
}

// Test that the view is reset when the dialog is dismissed.
TEST_F(AutofillDialogControllerImplTest, Dismiss_DeletesView) {
  // The view is only initialized after the dialog is shown.
  EXPECT_FALSE(controller_->HasDialogViewForTest());

  controller_->Show(u"Title", u"Description", u"Button", base::DoNothing());
  EXPECT_TRUE(controller_->HasDialogViewForTest());

  controller_->DismissForTest();
  EXPECT_FALSE(controller_->HasDialogViewForTest());
}

}  // namespace autofill
