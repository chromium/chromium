// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/installer_downloader/installer_downloader_infobar_delegate.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/infobars/confirm_infobar.h"
#include "chrome/browser/ui/views/infobars/infobar_view.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_feature.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/views/chrome_test_views_delegate.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace installer_downloader {
namespace {

using ::testing::StrictMock;

class InstallerDownloaderInfoBarDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  InstallerDownloaderInfoBarDelegateTest()
      : disable_animations_(gfx::AnimationTestApi::SetRichAnimationRenderMode(
            gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED)) {}

  ~InstallerDownloaderInfoBarDelegateTest() override = default;

 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    feature_list_.InitAndEnableFeatureWithParameters(
        kInstallerDownloader,
        {{kLearnMoreUrl.name, "https://example.com/learn_more"}});
    infobars::ContentInfoBarManager::CreateForWebContents(web_contents());

    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params(
        views::Widget::InitParams::Ownership::CLIENT_OWNS_WIDGET,
        views::Widget::InitParams::Type::TYPE_CONTROL);

    ASSERT_TRUE(web_contents());
    gfx::NativeView web_contents_native_view = web_contents()->GetNativeView();
    ASSERT_TRUE(web_contents_native_view);
    params.parent = web_contents_native_view;

    widget_->Init(std::move(params));
    widget_->Show();
  }

  void TearDown() override {
    if (widget_) {
      widget_->CloseNow();
      widget_.reset();
    }
    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<InstallerDownloaderInfoBarDelegate> CreateDelegate() {
    return std::make_unique<InstallerDownloaderInfoBarDelegate>(
        mock_accept_cb_.Get(), mock_cancel_cb_.Get());
  }

  StrictMock<base::MockCallback<base::OnceClosure>> mock_accept_cb_;
  StrictMock<base::MockCallback<base::OnceClosure>> mock_cancel_cb_;
  base::test::ScopedFeatureList feature_list_;

 private:
  gfx::AnimationTestApi::RenderModeResetter disable_animations_;
  ChromeTestViewsDelegate<> views_delegate_;
  std::unique_ptr<views::Widget> widget_;
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

TEST_F(InstallerDownloaderInfoBarDelegateTest, ClickAcceptButtonOnInfoBarView) {
  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents());
  ASSERT_TRUE(infobar_manager);

  infobars::InfoBar* added_infobar_ptr =
      InstallerDownloaderInfoBarDelegate::Show(
          infobar_manager, mock_accept_cb_.Get(), mock_cancel_cb_.Get());
  ASSERT_TRUE(added_infobar_ptr);

  ConfirmInfoBar* confirm_infobar_view =
      static_cast<ConfirmInfoBar*>(added_infobar_ptr);
  ASSERT_TRUE(confirm_infobar_view);

  views::MdTextButton* accept_button =
      confirm_infobar_view->ok_button_for_testing();
  ASSERT_TRUE(accept_button);
  ASSERT_TRUE(accept_button->GetVisible());
  ASSERT_TRUE(accept_button->GetEnabled());

  EXPECT_CALL_IN_SCOPE(
      mock_accept_cb_, Run,
      views::test::InteractionTestUtilSimulatorViews::PressButton(
          accept_button));
}

TEST_F(InstallerDownloaderInfoBarDelegateTest,
       ClickDismissButtonOnInfoBarView) {
  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents());
  ASSERT_TRUE(infobar_manager);

  infobars::InfoBar* added_infobar_ptr =
      InstallerDownloaderInfoBarDelegate::Show(
          infobar_manager, mock_accept_cb_.Get(), mock_cancel_cb_.Get());
  ASSERT_TRUE(added_infobar_ptr);

  ConfirmInfoBar* confirm_infobar_view =
      static_cast<ConfirmInfoBar*>(added_infobar_ptr);
  ASSERT_TRUE(confirm_infobar_view);

  views::View* dismiss_button_view = nullptr;
  for (views::View* child : confirm_infobar_view->children()) {
    if (child->GetProperty(views::kElementIdentifierKey) ==
        InfoBarView::kDismissButtonElementId) {
      dismiss_button_view = child;
      break;
    }
  }
  ASSERT_TRUE(dismiss_button_view);

  views::Button* dismiss_button = views::Button::AsButton(dismiss_button_view);
  ASSERT_TRUE(dismiss_button);
  ASSERT_TRUE(dismiss_button->GetVisible());
  ASSERT_TRUE(dismiss_button->GetEnabled());

  EXPECT_CALL_IN_SCOPE(
      mock_cancel_cb_, Run,
      views::test::InteractionTestUtilSimulatorViews::PressButton(
          dismiss_button));
}

}  // namespace
}  // namespace installer_downloader
