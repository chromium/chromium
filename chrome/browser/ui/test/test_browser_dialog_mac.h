// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TEST_TEST_BROWSER_DIALOG_MAC_H_
#define CHROME_BROWSER_UI_TEST_TEST_BROWSER_DIALOG_MAC_H_

namespace internal {

// Platform dependent fixture for TestBrowserDialog.  browser_tests is not built
// as an .app bundle, so windows from it cannot normally be activated.  But for
// interactive tests, dialogs need to be activated.  This hacks the process type
// so that they can be, and activates the application in case something already
// tried (but failed) to activate it before this.
void TestBrowserDialogInteractiveSetUp();

}  // namespace internal

#endif  // CHROME_BROWSER_UI_TEST_TEST_BROWSER_DIALOG_MAC_H_
