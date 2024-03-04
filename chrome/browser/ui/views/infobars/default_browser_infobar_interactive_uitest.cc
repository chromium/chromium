// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/startup/default_browser_prompt.h"
#include "chrome/browser/ui/startup/infobar_utils.h"
#include "chrome/browser/ui/views/infobars/confirm_infobar.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabContents);
}  // namespace

class DefaultBrowserInfobarInteractiveTest : public InteractiveBrowserTest {
 public:
  void SetUp() override { InteractiveBrowserTest::SetUp(); }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(DefaultBrowserInfobarInteractiveTest,
                       ShowsDefaultBrowserPrompt) {
  ShowPromptForTesting();
  RunTestSequence(
      WaitForShow(ConfirmInfoBar::kInfoBarElementId), FlushEvents(),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      WaitForHide(ConfirmInfoBar::kInfoBarElementId));
}
