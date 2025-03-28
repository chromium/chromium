// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_sharing/tab_sharing_infobar.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/tab_sharing/tab_sharing_ui.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/tab_sharing/tab_sharing_test_utils.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/test/browser_task_environment.h"
#include "media/capture/capture_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"

namespace {

using FocusTarget = ::TabSharingInfoBarDelegate::FocusTarget;
using TabRole = ::TabSharingInfoBarDelegate::TabRole;

const std::u16string kSharedTabName = u"example.com";
const std::u16string kAppName = u"sharing.com";
const std::u16string kSinkName = u"Living Room TV";

class MockTabSharingUIViews : public TabSharingUI {
 public:
  MockTabSharingUIViews() = default;
  MOCK_METHOD(void, StartSharing, (infobars::InfoBar * infobar));
  MOCK_METHOD(void, StopSharing, ());

  gfx::NativeViewId OnStarted(
      base::OnceClosure stop_callback,
      content::MediaStreamUI::SourceCallback source_callback,
      const std::vector<content::DesktopMediaID>& media_ids) override {
    return 0;
  }

  void OnRegionCaptureRectChanged(
      const std::optional<gfx::Rect>& region_capture_rect) override {}
};

class TestInfoBarManager : public infobars::InfoBarManager {
 public:
  TestInfoBarManager() = default;
  ~TestInfoBarManager() override = default;

  int GetActiveEntryID() override { return 0; }
  void OpenURL(const GURL& url, WindowOpenDisposition disposition) override {}
};

std::u16string GetInfoText(const TabSharingStatusMessageView& info_view) {
  std::u16string text;
  for (views::View* button_or_label : info_view.children()) {
    text += GetButtonOrLabelText(*button_or_label);
  }
  return text;
}

std::u16string GetInfoText(const TabSharingInfoBar& infobar) {
  const views::View& view = *infobar.GetStatusMessageViewForTesting();
  if (view.GetClassName() == "Label") {
    return std::u16string(static_cast<const views::Label&>(view).GetText());
  } else if (view.GetClassName() == "TabSharingStatusMessageView") {
    return GetInfoText(static_cast<const TabSharingStatusMessageView&>(view));
  }
  NOTREACHED();
}
}  // namespace

class TabSharingInfoBarTest : public testing::TestWithParam<bool> {
 public:
  struct Preferences {
    std::u16string shared_tab_name;
    std::u16string capturer_name;
    TabRole role;
    TabSharingInfoBarDelegate::TabShareType capture_type =
        TabSharingInfoBarDelegate::TabShareType::CAPTURE;
  };

  TabSharingInfoBarTest() {
    scoped_feature_list_.InitWithFeatureState(features::kTabCaptureInfobarLinks,
                                              GetParam());
  }

  TabSharingInfoBar* CreateInfobar(const Preferences& prefs) {
    return static_cast<TabSharingInfoBar*>(TabSharingInfoBarDelegate::Create(
        infobar_manager_.get(), nullptr, content::GlobalRenderFrameHostId(),
        content::GlobalRenderFrameHostId(), prefs.shared_tab_name,
        prefs.capturer_name, /*web_contents=*/nullptr, prefs.role,
        TabSharingInfoBarDelegate::ButtonState::ENABLED, FocusTarget(), true,
        &mock_ui, prefs.capture_type, false));
  }

 protected:
  void SetUp() override {
    ::testing::Test::SetUp();
    infobar_manager_ =
        std::make_unique<::testing::NiceMock<TestInfoBarManager>>();
  }

  void TearDown() override {
    infobar_manager_->ShutDown();
    ::testing::Test::TearDown();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;

 private:
  ChromeLayoutProvider layout_provider_;

  MockTabSharingUIViews mock_ui;
  std::unique_ptr<TestInfoBarManager> infobar_manager_;
};

// Instantiate tests for the TabCaptureInfobarLinks feature being
// active/inactive.
INSTANTIATE_TEST_SUITE_P(, TabSharingInfoBarTest, testing::Bool());

// Test that the infobar on the capturing tab has the correct text:
// "|icon| Sharing this tab to |app|"
TEST_P(TabSharingInfoBarTest, InfobarOnCapturingTab) {
  TabSharingInfoBar* const infobar =
      CreateInfobar({.shared_tab_name = std::u16string(),
                     .capturer_name = kAppName,
                     .role = TabRole::kCapturingTab});

  EXPECT_EQ(GetInfoText(*infobar),
            l10n_util::GetStringFUTF16(
                IDS_TAB_SHARING_INFOBAR_SHARING_ANOTHER_UNTITLED_TAB_LABEL,
                kAppName));
}

// Test that the infobar on the shared tab has the correct text:
// "Sharing this tab to |app|"
TEST_P(TabSharingInfoBarTest, InfobarOnCapturedTab) {
  TabSharingInfoBar* const infobar =
      CreateInfobar({.shared_tab_name = std::u16string(),
                     .capturer_name = kAppName,
                     .role = TabRole::kCapturedTab});

  EXPECT_EQ(GetInfoText(*infobar),
            l10n_util::GetStringFUTF16(
                IDS_TAB_SHARING_INFOBAR_SHARING_CURRENT_TAB_LABEL, kAppName));
}

// Test that the infobar on another not share tab has the correct text:
// Sharing |shared_tab| to |app|
TEST_P(TabSharingInfoBarTest, InfobarOnNotSharedTab) {
  TabSharingInfoBar* const infobar =
      CreateInfobar({.shared_tab_name = kSharedTabName,
                     .capturer_name = kAppName,
                     .role = TabRole::kOtherTab});
  EXPECT_EQ(GetInfoText(*infobar),
            l10n_util::GetStringFUTF16(
                IDS_TAB_SHARING_INFOBAR_SHARING_ANOTHER_TAB_LABEL,
                kSharedTabName, kAppName));
}

// Test that if the app preferred self-capture, but the user either chose
// another tab, or chose the current tab but then switched to sharing another,
// then the infobar has the correct text:
// Sharing |shared_tab| to |app|
TEST_P(TabSharingInfoBarTest,
       InfobarOnCapturingTabIfCapturedAnotherTabButSelfCapturePreferred) {
  TabSharingInfoBar* const infobar =
      CreateInfobar({.shared_tab_name = std::u16string(),
                     .capturer_name = kAppName,
                     .role = TabRole::kCapturedTab});

  EXPECT_EQ(GetInfoText(*infobar),
            l10n_util::GetStringFUTF16(
                IDS_TAB_SHARING_INFOBAR_SHARING_CURRENT_TAB_LABEL, kAppName));
}

// Test that the infobar on another not cast tab has the correct text:
// "Casting |tab_being_cast| to |sink|"
TEST_P(TabSharingInfoBarTest, InfobarOnNotCastTab) {
  Preferences preferences = {
      .shared_tab_name = kSharedTabName,
      .capturer_name = kSinkName,
      .role = TabRole::kOtherTab,
      .capture_type = TabSharingInfoBarDelegate::TabShareType::CAST};
  TabSharingInfoBar* const infobar = CreateInfobar(preferences);
  EXPECT_EQ(GetInfoText(*infobar),
            l10n_util::GetStringFUTF16(
                IDS_TAB_CASTING_INFOBAR_CASTING_ANOTHER_TAB_LABEL,
                kSharedTabName, kSinkName));

  // Without sink name.
  preferences.capturer_name = std::u16string();
  TabSharingInfoBar* const infobar2 = CreateInfobar(preferences);
  EXPECT_EQ(
      GetInfoText(*infobar2),
      l10n_util::GetStringFUTF16(
          IDS_TAB_CASTING_INFOBAR_CASTING_ANOTHER_TAB_NO_DEVICE_NAME_LABEL,
          kSharedTabName));
}

// Test that the infobar on the tab being cast has the correct text:
// "Casting this tab to |sink|"
TEST_P(TabSharingInfoBarTest, InfobarOnCastTab) {
  Preferences preferences = {
      .shared_tab_name = std::u16string(),
      .capturer_name = kSinkName,
      .role = TabRole::kCapturedTab,
      .capture_type = TabSharingInfoBarDelegate::TabShareType::CAST};
  TabSharingInfoBar* const infobar = CreateInfobar(preferences);
  EXPECT_EQ(GetInfoText(*infobar),
            l10n_util::GetStringFUTF16(
                IDS_TAB_CASTING_INFOBAR_CASTING_CURRENT_TAB_LABEL, kSinkName));

  // Without sink name.
  preferences.capturer_name = std::u16string();
  TabSharingInfoBar* const infobar2 = CreateInfobar(preferences);
  EXPECT_EQ(
      GetInfoText(*infobar2),
      l10n_util::GetStringUTF16(
          IDS_TAB_CASTING_INFOBAR_CASTING_CURRENT_TAB_NO_DEVICE_NAME_LABEL));
}
