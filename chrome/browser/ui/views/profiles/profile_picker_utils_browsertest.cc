// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_utils.h"

#include <memory>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "url/gurl.h"

namespace {

using ProfilePickerUtilsBrowserTest = InProcessBrowserTest;

}  // namespace

IN_PROC_BROWSER_TEST_F(ProfilePickerUtilsBrowserTest, OpenLearnMorePopup) {
  blink::mojom::WindowFeatures window_features;

  auto contents = content::WebContents::Create(
      content::WebContents::CreateParams(browser()->GetProfile()));
  content::WebContents* raw_contents = contents.get();

  ui_test_utils::BrowserCreatedObserver observer;

  OpenLearnMorePopup(browser()->GetProfile(), std::move(contents),
                     /*target_url=*/GURL(url::kAboutBlankURL), window_features);

  BrowserWindowInterface* popup_browser = observer.Wait();
  ASSERT_NE(popup_browser, nullptr);

  EXPECT_NE(popup_browser, browser());
  EXPECT_EQ(popup_browser->GetType(), BrowserWindowInterface::Type::TYPE_POPUP);
  EXPECT_EQ(popup_browser->GetProfile(), browser()->GetProfile());
  EXPECT_EQ(popup_browser->GetTabStripModel()->GetActiveWebContents(),
            raw_contents);
}
