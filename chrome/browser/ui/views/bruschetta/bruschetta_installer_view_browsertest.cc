// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bruschetta/bruschetta_installer_view.h"

#include <memory>

#include "chrome/browser/ash/bruschetta/bruschetta_installer.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using testing::AnyNumber;
using testing::AtLeast;

namespace bruschetta {
namespace {

class BruschettaInstallerMock : public bruschetta::BruschettaInstaller {
 public:
  MOCK_METHOD(void, Cancel, ());
  MOCK_METHOD(void, Install, (std::string, std::string));
  MOCK_METHOD(void, AddObserver, (Observer*));
  MOCK_METHOD(void, RemoveObserver, (Observer*));

  MOCK_METHOD(const base::GUID&, GetDownloadGuid, (), (const));

  MOCK_METHOD(void,
              DownloadStarted,
              (const std::string& guid,
               download::DownloadParams::StartResult result));
  MOCK_METHOD(void, DownloadFailed, ());
  MOCK_METHOD(void,
              DownloadSucceeded,
              (const download::CompletionInfo& completion_info));
};

class BruschettaInstallerViewBrowserTest : public DialogBrowserTest {
 public:
  BruschettaInstallerViewBrowserTest() = default;

  // Disallow copy and assign.
  BruschettaInstallerViewBrowserTest(
      const BruschettaInstallerViewBrowserTest&) = delete;
  BruschettaInstallerViewBrowserTest& operator=(
      const BruschettaInstallerViewBrowserTest&) = delete;

  void SetUpOnMainThread() override {}

  void ShowUi(const std::string& name) override {
    BruschettaInstallerView::Show(browser()->profile(), GetBruschettaAlphaId());
    view_ = BruschettaInstallerView::GetActiveViewForTesting();

    ASSERT_NE(nullptr, view_);
    ASSERT_FALSE(view_->GetWidget()->IsClosed());
    auto mock_installer = std::make_unique<BruschettaInstallerMock>();
    installer_ = mock_installer.get();
    view_->set_installer_for_testing(std::move(mock_installer));
    EXPECT_CALL(*installer_, AddObserver).Times(AnyNumber());
  }

  BruschettaInstallerView* view_;
  BruschettaInstallerMock* installer_;
};

IN_PROC_BROWSER_TEST_F(BruschettaInstallerViewBrowserTest, Show) {
  ShowUi("default");
  EXPECT_NE(nullptr, view_->GetOkButton());
  EXPECT_NE(nullptr, view_->GetCancelButton());
  EXPECT_EQ(
      view_->GetPrimaryMessage(),
      l10n_util::GetStringUTF16(IDS_BRUSCHETTA_INSTALLER_CONFIRMATION_TITLE));
}

IN_PROC_BROWSER_TEST_F(BruschettaInstallerViewBrowserTest,
                       CancelOnPromptScreen) {
  ShowUi("default");
  view_->CancelDialog();
  ASSERT_TRUE(view_->GetWidget()->IsClosed());
}

IN_PROC_BROWSER_TEST_F(BruschettaInstallerViewBrowserTest, InstallThenCancel) {
  ShowUi("default");
  EXPECT_CALL(*installer_, Install);
  EXPECT_CALL(*installer_, Cancel).Times(AtLeast(1));

  view_->AcceptDialog();
  EXPECT_EQ(nullptr, view_->GetOkButton());
  EXPECT_NE(nullptr, view_->GetCancelButton());
  EXPECT_EQ(view_->GetPrimaryMessage(),
            l10n_util::GetStringUTF16(IDS_BRUSCHETTA_INSTALLER_ONGOING_TITLE));

  view_->CancelDialog();
  ASSERT_TRUE(view_->GetWidget()->IsClosed());
}

IN_PROC_BROWSER_TEST_F(BruschettaInstallerViewBrowserTest, InstallThenError) {
  ShowUi("default");
  EXPECT_CALL(*installer_, Install);

  view_->AcceptDialog();
  EXPECT_EQ(nullptr, view_->GetOkButton());
  EXPECT_NE(nullptr, view_->GetCancelButton());
  EXPECT_EQ(view_->GetPrimaryMessage(),
            l10n_util::GetStringUTF16(IDS_BRUSCHETTA_INSTALLER_ONGOING_TITLE));

  view_->Error(BruschettaInstallResult::kStartVmFailed);
  EXPECT_EQ(nullptr, view_->GetOkButton());
  EXPECT_NE(nullptr, view_->GetCancelButton());
  EXPECT_EQ(view_->GetPrimaryMessage(),
            l10n_util::GetStringUTF16(IDS_BRUSCHETTA_INSTALLER_ERROR_TITLE));

  EXPECT_CALL(*installer_, Cancel).Times(AtLeast(1));
  view_->CancelDialog();
  ASSERT_TRUE(view_->GetWidget()->IsClosed());
}

IN_PROC_BROWSER_TEST_F(BruschettaInstallerViewBrowserTest, InstallThenSuccess) {
  ShowUi("default");
  EXPECT_CALL(*installer_, Install);
  EXPECT_CALL(*installer_, Cancel).Times(0);

  view_->AcceptDialog();
  EXPECT_EQ(nullptr, view_->GetOkButton());
  EXPECT_NE(nullptr, view_->GetCancelButton());
  EXPECT_EQ(view_->GetPrimaryMessage(),
            l10n_util::GetStringUTF16(IDS_BRUSCHETTA_INSTALLER_ONGOING_TITLE));
  auto first_message = view_->GetSecondaryMessage();

  // Check that state changes update the progress message.
  view_->StateChanged(bruschetta::BruschettaInstaller::State::kStartVm);
  EXPECT_EQ(nullptr, view_->GetOkButton());
  EXPECT_NE(nullptr, view_->GetCancelButton());
  EXPECT_EQ(view_->GetPrimaryMessage(),
            l10n_util::GetStringUTF16(IDS_BRUSCHETTA_INSTALLER_ONGOING_TITLE));
  EXPECT_NE(first_message, view_->GetSecondaryMessage());

  view_->OnInstallationEnded();

  // We close the installer upon completion since we switch to a terminal
  // window to complete the install.
  ASSERT_TRUE(view_->GetWidget()->IsClosed());
}

}  // namespace
}  // namespace bruschetta
