// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/optional.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

class AvatarToolbarButtonBrowserTest : public InProcessBrowserTest {
 public:
  AvatarToolbarButtonBrowserTest() = default;
  AvatarToolbarButtonBrowserTest(const AvatarToolbarButtonBrowserTest&) =
      delete;
  AvatarToolbarButtonBrowserTest& operator=(
      const AvatarToolbarButtonBrowserTest&) = delete;
  ~AvatarToolbarButtonBrowserTest() override = default;

  // Returns the window count in avatar button text, if it exists.
  base::Optional<int> GetWindowCountInAvatarButtonText(Browser* browser) {
    std::u16string button_text = BrowserView::GetBrowserViewForBrowser(browser)
                                     ->toolbar()
                                     ->avatar_->GetText();

    size_t before_number = button_text.find('(');
    if (before_number == std::string::npos)
      return base::Optional<int>();

    size_t after_number = button_text.find(')');
    EXPECT_NE(std::string::npos, after_number);

    std::u16string number_text =
        button_text.substr(before_number + 1, after_number - before_number - 1);
    int window_count;
    return base::StringToInt(number_text, &window_count)
               ? base::Optional<int>(window_count)
               : base::Optional<int>();
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
