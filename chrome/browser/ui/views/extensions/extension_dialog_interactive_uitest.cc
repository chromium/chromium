// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_dialog.h"

#include "build/build_config.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/test/extension_test_message_listener.h"
#include "ui/base/test/ui_controls.h"

namespace {

class ExtensionDialogUiTest : public extensions::ExtensionBrowserTest {
 public:
  ExtensionDialogUiTest() = default;
  ~ExtensionDialogUiTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionDialogUiTest);
};

}  // namespace

#if defined(OS_MACOSX)
// Focusing or input is not completely working on Mac: http://crbug.com/824418
#define MAYBE_TabFocusLoop DISABLED_TabFocusLoop
#else
#define MAYBE_TabFocusLoop TabFocusLoop
#endif
IN_PROC_BROWSER_TEST_F(ExtensionDialogUiTest, MAYBE_TabFocusLoop) {
  ExtensionTestMessageListener init_listener("ready", false /* will_reply */);
  ExtensionTestMessageListener button1_focus_listener("button1-focused", false);
  ExtensionTestMessageListener button2_focus_listener("button2-focused", false);
  ExtensionTestMessageListener button3_focus_listener("button3-focused", false);

  // Load an extension for the test.
  scoped_refptr<const extensions::Extension> extension =
      LoadExtension(test_data_dir_.AppendASCII("uitest/tab_traversal"));
  ASSERT_TRUE(extension.get());

  // Open ExtensionDialog, whose initial page is the extension's main.html.
  // The main.html contains three buttons.
  ExtensionDialog* dialog = ExtensionDialog::Show(
      extension->url().Resolve("main.html"),
      browser()->window()->GetNativeWindow(), browser()->profile(),
      NULL, 300, 300, 300, 300, base::string16(), NULL);
  ASSERT_TRUE(dialog);
  ASSERT_TRUE(init_listener.WaitUntilSatisfied());

  // Focus the second button.
  ASSERT_TRUE(
      content::ExecuteScript(dialog->host()->host_contents(),
                             "document.querySelector('#button2').focus()"));
  ASSERT_TRUE(button2_focus_listener.WaitUntilSatisfied());

  // Pressing TAB should focus the third(last) button.
  ASSERT_TRUE(ui_controls::SendKeyPress(
                  browser()->window()->GetNativeWindow(),
                  ui::VKEY_TAB, false, false, false, false));
  ASSERT_TRUE(button3_focus_listener.WaitUntilSatisfied());

  // Pressing TAB again should focus the first button.
  ASSERT_TRUE(ui_controls::SendKeyPress(
                  browser()->window()->GetNativeWindow(),
                  ui::VKEY_TAB, false, false, false, false));
  ASSERT_TRUE(button1_focus_listener.WaitUntilSatisfied());

  // Pressing Shift+TAB on the first button should focus the last button.
  ASSERT_TRUE(ui_controls::SendKeyPress(
                  browser()->window()->GetNativeWindow(),
                  ui::VKEY_TAB, false, true, false, false));
  ASSERT_TRUE(button3_focus_listener.WaitUntilSatisfied());
};
