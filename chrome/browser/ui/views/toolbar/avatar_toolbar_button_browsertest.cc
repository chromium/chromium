// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class AvatarToolbarButtonBrowserTest : public InProcessBrowserTest {
 public:
  AvatarToolbarButtonBrowserTest() = default;
  AvatarToolbarButtonBrowserTest(const AvatarToolbarButtonBrowserTest&) =
      delete;
  AvatarToolbarButtonBrowserTest& operator=(
      const AvatarToolbarButtonBrowserTest&) = delete;
  ~AvatarToolbarButtonBrowserTest() override = default;

  AvatarToolbarButton* GetAvatarToolbarButton(Browser* browser) {
    return BrowserView::GetBrowserViewForBrowser(browser)->toolbar()->avatar_;
  }

  // Returns the window count in avatar button text, if it exists.
  absl::optional<int> GetWindowCountInAvatarButtonText(Browser* browser) {
    std::u16string button_text = GetAvatarToolbarButton(browser)->GetText();

    size_t before_number = button_text.find('(');
    if (before_number == std::string::npos)
      return absl::optional<int>();

    size_t after_number = button_text.find(')');
    EXPECT_NE(std::string::npos, after_number);

    std::u16string number_text =
        button_text.substr(before_number + 1, after_number - before_number - 1);
    int window_count;
    return base::StringToInt(number_text, &window_count)
               ? absl::optional<int>(window_count)
               : absl::optional<int>();
  }
};

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest, IncogntioWindowCount) {
  Profile* profile = browser()->profile();
  Browser* browser1 = CreateIncognitoBrowser(profile);
  EXPECT_FALSE(GetWindowCountInAvatarButtonText(browser1).has_value());

  Browser* browser2 = CreateIncognitoBrowser(profile);
  EXPECT_EQ(2, *GetWindowCountInAvatarButtonText(browser1));
  EXPECT_EQ(2, *GetWindowCountInAvatarButtonText(browser2));

  CloseBrowserSynchronously(browser2);
  EXPECT_FALSE(GetWindowCountInAvatarButtonText(browser1).has_value());
}

// TODO(https://crbug.com/1179717): Enable the test for ChromeOS.
// Note that |CreateGuestBrowser| does not create a Guest browser for ChromeOS
// and Chrome OS Guest does not have window counter.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest, GuestWindowCount) {
  Browser* browser1 = CreateGuestBrowser();
  EXPECT_FALSE(GetWindowCountInAvatarButtonText(browser1).has_value());

  Browser* browser2 = CreateGuestBrowser();
  EXPECT_EQ(2, *GetWindowCountInAvatarButtonText(browser1));
  EXPECT_EQ(2, *GetWindowCountInAvatarButtonText(browser2));

  CloseBrowserSynchronously(browser2);
  EXPECT_FALSE(GetWindowCountInAvatarButtonText(browser1).has_value());
}
#endif

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest, DefaultBrowser) {
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser());
  ASSERT_TRUE(avatar);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // No avatar button is shown in normal Ash windows.
  EXPECT_FALSE(avatar->GetVisible());
#else
  EXPECT_TRUE(avatar->GetVisible());
  EXPECT_TRUE(avatar->GetEnabled());
#endif
}

IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest, IncognitoBrowser) {
  Browser* browser1 = CreateIncognitoBrowser(browser()->profile());
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser1);
  ASSERT_TRUE(avatar);
  // Incognito browsers always show an enabled avatar button.
  EXPECT_TRUE(avatar->GetVisible());
  EXPECT_TRUE(avatar->GetEnabled());
}

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(AvatarToolbarButtonBrowserTest, SigninBrowser) {
  // Create an Incognito browser first.
  CreateIncognitoBrowser(browser()->profile());
  // Create a portal signin browser which will not be the Incognito browser.
  Profile::OTRProfileID profile_id(
      Profile::OTRProfileID::CreateUniqueForCaptivePortal());
  Browser* browser1 = Browser::Create(Browser::CreateParams(
      browser()->profile()->GetOffTheRecordProfile(profile_id,
                                                   /*create_if_needed=*/true),
      true));
  AddBlankTabAndShow(browser1);
  AvatarToolbarButton* avatar = GetAvatarToolbarButton(browser1);
  ASSERT_TRUE(avatar);
  // On ChromeOS (Ash and Lacros), captive portal signin windows show a
  // disabled avatar button to indicate that the window is incognito.
  EXPECT_TRUE(avatar->GetVisible());
  EXPECT_FALSE(avatar->GetEnabled());
}
#endif
