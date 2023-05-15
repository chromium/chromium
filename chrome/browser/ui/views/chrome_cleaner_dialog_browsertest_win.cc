// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_cleaner_dialog_win.h"

#include <memory>

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_dialog_controller_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/mock_chrome_cleaner_controller_win.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::NiceMock;
using ::testing::Return;

namespace {

class MockChromeCleanerDialogController
    : public safe_browsing::ChromeCleanerDialogController {
 public:
  MOCK_METHOD(void, DialogShown, (), (override));
  MOCK_METHOD(void, Accept, (bool), (override));
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(void, Close, (), (override));
  MOCK_METHOD(void, ClosedWithoutUserInteraction, (), (override));
  MOCK_METHOD(void, DetailsButtonClicked, (bool), (override));
  MOCK_METHOD(void, SetLogsEnabled, (bool), (override));
  MOCK_METHOD(bool, LogsEnabled, (), (override));
  MOCK_METHOD(bool, LogsManaged, (), (override));
};

class ChromeCleanerDialogTest : public DialogBrowserTest {
 public:
  ChromeCleanerDialogTest()
      : mock_dialog_controller_(
            std::make_unique<NiceMock<MockChromeCleanerDialogController>>()),
        mock_cleaner_controller_(
            std::make_unique<
                NiceMock<safe_browsing::MockChromeCleanerController>>()) {
    ON_CALL(*mock_dialog_controller_, LogsEnabled())
        .WillByDefault(Return(true));
    ON_CALL(*mock_cleaner_controller_, state())
        .WillByDefault(
            Return(safe_browsing::ChromeCleanerController::State::kInfected));
  }

  ChromeCleanerDialogTest(const ChromeCleanerDialogTest&) = delete;
  ChromeCleanerDialogTest& operator=(const ChromeCleanerDialogTest&) = delete;

  void ShowUi(const std::string& name) override {
    chrome::ShowChromeCleanerPrompt(browser(), mock_dialog_controller_.get(),
                                    mock_cleaner_controller_.get());
  }

 protected:
  // Since the DialogBrowserTest can be run interactively, we use NiceMock here
  // to suppress warnings about uninteresting calls.
  std::unique_ptr<NiceMock<MockChromeCleanerDialogController>>
      mock_dialog_controller_;
  std::unique_ptr<NiceMock<safe_browsing::MockChromeCleanerController>>
      mock_cleaner_controller_;
};

IN_PROC_BROWSER_TEST_F(ChromeCleanerDialogTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

}  // namespace
