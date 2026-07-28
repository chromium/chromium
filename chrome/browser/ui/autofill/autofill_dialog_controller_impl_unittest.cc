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

using ::testing::IsEmpty;
using ::testing::NiceMock;

class AutofillDialogControllerImplTest
    : public ChromeRenderViewHostTestHarness {
 public:
  AutofillDialogControllerImplTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

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
    if (!mock_view_) {
      mock_view_ = std::make_unique<NiceMock<MockAutofillDialogView>>();
      mock_view_ptr_ = mock_view_.get();
    }
    return std::move(mock_view_);
  }

  std::unique_ptr<AutofillDialogControllerImpl> controller_;
  std::unique_ptr<NiceMock<MockAutofillDialogView>> mock_view_;
  raw_ptr<NiceMock<MockAutofillDialogView>> mock_view_ptr_ = nullptr;
};

// Test that the dialog is shown.
TEST_F(AutofillDialogControllerImplTest, ShowDialog) {
  EXPECT_CALL(*mock_view_ptr_, Show());
  EXPECT_CALL(*mock_view_ptr_, ShowLoadingDialog()).Times(0);
  controller_->Show(u"Title", u"Description", u"Button", base::DoNothing());

  EXPECT_EQ(u"Title", controller_->GetTitleText());
  EXPECT_EQ(u"Description", controller_->GetDescriptionText());
  EXPECT_EQ(u"Button", controller_->GetButtonText());
}

// Test that the loading dialog is shown.
TEST_F(AutofillDialogControllerImplTest, ShowLoadingDialog) {
  EXPECT_CALL(*mock_view_ptr_, Show()).Times(0);
  EXPECT_CALL(*mock_view_ptr_, ShowLoadingDialog());
  controller_->ShowLoadingDialog(u"Title", base::Seconds(2));

  EXPECT_EQ(u"Title", controller_->GetTitleText());
  EXPECT_THAT(controller_->GetDescriptionText(), IsEmpty());
  EXPECT_THAT(controller_->GetButtonText(), IsEmpty());
}

// Test that `Dismiss` hides the dialog view immediately if `min_time` has
// elapsed.
TEST_F(AutofillDialogControllerImplTest, DismissLoading_AfterMinTime) {
  EXPECT_CALL(*mock_view_ptr_, Show()).Times(0);
  EXPECT_CALL(*mock_view_ptr_, ShowLoadingDialog());

  controller_->ShowLoadingDialog(u"Title", base::Seconds(2));

  task_environment()->FastForwardBy(base::Seconds(2));

  // Over the min_time, should dismiss immediately.
  EXPECT_CALL(*mock_view_ptr_, Dismiss());
  controller_->Dismiss();
}

// Test that `Dismiss` waits until `min_time` passed before dismissing.
TEST_F(AutofillDialogControllerImplTest, DismissLoading_BeforeMinTime) {
  EXPECT_CALL(*mock_view_ptr_, ShowLoadingDialog());
  controller_->ShowLoadingDialog(u"Title", base::Seconds(2));

  task_environment()->FastForwardBy(base::Seconds(1));

  // Under the min_time, should NOT dismiss yet.
  EXPECT_CALL(*mock_view_ptr_, Dismiss()).Times(0);
  controller_->Dismiss();

  // Fast forward by exactly the remaining time (1s). It should dismiss.
  EXPECT_CALL(*mock_view_ptr_, Dismiss());
  task_environment()->FastForwardBy(base::Seconds(1));
}

// Test that multiple `Dismiss` calls before `min_time` still correctly dismiss
// just once.
TEST_F(AutofillDialogControllerImplTest, DismissLoading_MultipleTimes) {
  EXPECT_CALL(*mock_view_ptr_, ShowLoadingDialog());
  controller_->ShowLoadingDialog(u"Title", base::Seconds(2));

  controller_->Dismiss();
  controller_->Dismiss();
  controller_->Dismiss();

  EXPECT_CALL(*mock_view_ptr_, Dismiss());
  task_environment()->FastForwardBy(base::Seconds(2));
}

// Test that we can properly show and dismiss the loading dialog consecutively.
TEST_F(AutofillDialogControllerImplTest, ShowAndDismissLoadingDialogTwice) {
  // First show and dismiss.
  EXPECT_CALL(*mock_view_ptr_, ShowLoadingDialog());
  controller_->ShowLoadingDialog(u"Title1", base::Seconds(2));

  task_environment()->FastForwardBy(base::Seconds(2));
  EXPECT_CALL(*mock_view_ptr_, Dismiss());
  controller_->Dismiss();
  EXPECT_FALSE(controller_->HasDialogViewForTest());

  // Pre-populate mock_view_ for the second dialog so we can set expectations.
  mock_view_ = std::make_unique<NiceMock<MockAutofillDialogView>>();
  mock_view_ptr_ = mock_view_.get();

  EXPECT_CALL(*mock_view_ptr_, ShowLoadingDialog());
  controller_->ShowLoadingDialog(u"Title2", base::Seconds(2));

  task_environment()->FastForwardBy(base::Seconds(2));
  EXPECT_CALL(*mock_view_ptr_, Dismiss());
  controller_->Dismiss();
  EXPECT_FALSE(controller_->HasDialogViewForTest());
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
