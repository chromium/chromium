// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/installer_downloader/installer_downloader_infobar_delegate.h"

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_feature.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"

namespace installer_downloader {
namespace {

class InstallerDownloaderInfoBarDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  InstallerDownloaderInfoBarDelegateTest() = default;

 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    feature_list_.InitAndEnableFeatureWithParameters(
        kInstallerDownloader,
        {{kLearnMoreUrl.name, "https://example.com/learn_more"}});
    infobars::ContentInfoBarManager::CreateForWebContents(web_contents());
  }
  std::unique_ptr<InstallerDownloaderInfoBarDelegate> CreateDelegate() {
    return std::make_unique<InstallerDownloaderInfoBarDelegate>(
        mock_accept_cb_.Get(), mock_cancel_cb_.Get());
  }
  base::MockCallback<base::OnceClosure> mock_accept_cb_;
  base::MockCallback<base::OnceClosure> mock_cancel_cb_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(InstallerDownloaderInfoBarDelegateTest, CheckInfoBarProperties) {
  auto delegate = CreateDelegate();

  EXPECT_EQ(infobars::InfoBarDelegate::INSTALLER_DOWNLOADER_INFOBAR_DELEGATE,
            delegate->GetIdentifier());
  // Check for icon.
  EXPECT_TRUE(delegate->GetIcon().IsVectorIcon());
  // Check for infobar text.
  EXPECT_FALSE(delegate->GetMessageText().empty());
  EXPECT_FALSE(delegate->GetLinkText().empty());
  // Check for infobar button.
  EXPECT_EQ(ConfirmInfoBarDelegate::BUTTON_OK, delegate->GetButtons());
}

TEST_F(InstallerDownloaderInfoBarDelegateTest, LinkClicked) {
  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents());
  ASSERT_TRUE(infobar_manager);

  std::unique_ptr<InstallerDownloaderInfoBarDelegate> delegate =
      CreateDelegate();
  auto test_infobar = std::make_unique<infobars::InfoBar>(std::move(delegate));
  infobars::InfoBar* added_infobar_ptr =
      infobar_manager->AddInfoBar(std::move(test_infobar));
  ASSERT_TRUE(added_infobar_ptr);

  EXPECT_FALSE(added_infobar_ptr->delegate()->LinkClicked(
      WindowOpenDisposition::CURRENT_TAB));
}

TEST_F(InstallerDownloaderInfoBarDelegateTest, AddInfoBarToManager) {
  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents());
  ASSERT_TRUE(infobar_manager);

  // Initially, there is no infobar.
  EXPECT_EQ(0u, infobar_manager->infobars().size());
  std::unique_ptr<InstallerDownloaderInfoBarDelegate> delegate =
      CreateDelegate();
  auto test_infobar = std::make_unique<infobars::InfoBar>(std::move(delegate));

  // Verify that an infobar was added.
  infobars::InfoBar* added_infobar_ptr =
      infobar_manager->AddInfoBar(std::move(test_infobar));
  ASSERT_TRUE(added_infobar_ptr);

  // Verify the infobar was actually added and has the correct delegate.
  EXPECT_EQ(1u, infobar_manager->infobars().size());
  EXPECT_EQ(added_infobar_ptr, infobar_manager->infobars()[0]);
  EXPECT_EQ(infobars::InfoBarDelegate::INSTALLER_DOWNLOADER_INFOBAR_DELEGATE,
            added_infobar_ptr->delegate()->GetIdentifier());
}

// TODO(crbug.com/412697757): Add views test for clicking cancel/accept on
// infobar.
TEST_F(InstallerDownloaderInfoBarDelegateTest, CancelClicked) {
  auto delegate = CreateDelegate();
  EXPECT_CALL(mock_cancel_cb_, Run).Times(1);
  delegate->InfoBarDismissed();
}

}  // namespace
}  // namespace installer_downloader
