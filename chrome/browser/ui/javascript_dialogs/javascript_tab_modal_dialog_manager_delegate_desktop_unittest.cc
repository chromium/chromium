// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/javascript_dialogs/javascript_tab_modal_dialog_manager_delegate_desktop.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/tabs/public/tab_interface.h"
#include "testing/gtest/include/gtest/gtest.h"

class JavaScriptTabModalDialogManagerDelegateDesktopTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    // The harness's WebContents has no TabFeatures attached, so
    // MaybeGetFromContents() returns nullptr — this is the "detached from
    // tab" state that triggers the crashes regression-tested below. If the
    // harness ever starts attaching TabFeatures, this ASSERT fails loudly
    // and signals that these tests stopped exercising the null-tab path.
    ASSERT_FALSE(tabs::TabInterface::MaybeGetFromContents(web_contents()));
  }
};

// Regression test: IsApp() must not crash when the WebContents has been
// detached from its tab (race between tab-close and onbeforeunload dialog).
// See crbug.com/40727952.
TEST_F(JavaScriptTabModalDialogManagerDelegateDesktopTest,
       IsAppDoesNotCrashWithDetachedWebContents) {
  JavaScriptTabModalDialogManagerDelegateDesktop delegate(web_contents());

  // Before the fix: null deref at tab->GetBrowserWindowInterface().
  EXPECT_FALSE(delegate.IsApp());
}

// Regression test: SetTabNeedsAttention() has the same class of null-deref
// bug as IsApp() — it checks `browser` for null after dereferencing `tab`,
// but `tab` itself can be nullptr.
TEST_F(JavaScriptTabModalDialogManagerDelegateDesktopTest,
       SetTabNeedsAttentionDoesNotCrashWithDetachedWebContents) {
  JavaScriptTabModalDialogManagerDelegateDesktop delegate(web_contents());

  delegate.SetTabNeedsAttention(true);
  delegate.SetTabNeedsAttention(false);
}

// Regression test: CanShowModalUI() called GetFromContents() (which CHECK-
// fails on a detached WebContents) and relied on a subsequent `tab && ...`
// check — that null check was dead code under the old API. After switching
// to MaybeGetFromContents(), the null check becomes meaningful and the
// method must return false instead of crashing.
TEST_F(JavaScriptTabModalDialogManagerDelegateDesktopTest,
       CanShowModalUIDoesNotCrashWithDetachedWebContents) {
  JavaScriptTabModalDialogManagerDelegateDesktop delegate(web_contents());

  EXPECT_FALSE(delegate.CanShowModalUI());
}
