// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_dialog.h"

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/content_test_utils.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/extension_test_message_listener.h"

namespace {

using ExtensionDialogTest = extensions::ExtensionBrowserTest;

IN_PROC_BROWSER_TEST_F(ExtensionDialogTest, TextInputViaKeyEvent) {
  ExtensionTestMessageListener init_listener("ready", /*will_reply=*/false);

  scoped_refptr<const extensions::Extension> extension =
      LoadExtension(test_data_dir_.AppendASCII("uitest/tab_traversal"));
  ASSERT_TRUE(extension.get());

  ExtensionDialog::InitParams params(gfx::Size(400, 300));
  params.is_modal = true;
  params.min_size = {400, 300};
  auto* dialog = ExtensionDialog::Show(
      extension->url().Resolve("main.html"),
      browser()->window()->GetNativeWindow(), browser()->profile(),
      /*web_contents=*/nullptr, /*observer=*/nullptr, params);
  ASSERT_TRUE(dialog);
  ASSERT_TRUE(init_listener.WaitUntilSatisfied());

  TestTextInputViaKeyEvent(dialog->host()->host_contents());
}

}  // namespace
