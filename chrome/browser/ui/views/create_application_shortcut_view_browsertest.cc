// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/create_application_shortcut_view.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class CreateAppShortcutDialogTest : public DialogBrowserTest,
                                    public testing::WithParamInterface<bool> {
 public:
  CreateAppShortcutDialogTest() = default;
  CreateAppShortcutDialogTest(const CreateAppShortcutDialogTest&) = delete;
  CreateAppShortcutDialogTest& operator=(const CreateAppShortcutDialogTest&) =
      delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    constrained_window::CreateBrowserModalDialogViews(
        new CreateChromeApplicationShortcutView(browser()->profile(),
                                                /*is_extension=*/GetParam(),
                                                base::DoNothing()),
        browser()->window()->GetNativeWindow())
        ->Show();
  }
};

IN_PROC_BROWSER_TEST_P(CreateAppShortcutDialogTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(All, CreateAppShortcutDialogTest, ::testing::Bool());
