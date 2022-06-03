// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_FILE_HANDLING_PERMISSION_REQUEST_DIALOG_TEST_API_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_FILE_HANDLING_PERMISSION_REQUEST_DIALOG_TEST_API_H_

namespace web_app {

class FileHandlingPermissionRequestDialogTestApi {
 public:
  FileHandlingPermissionRequestDialogTestApi() = delete;

  // Returns whether a dialog is currently showing.
  static bool IsShowing();

  // Simulates a user choice in the most recently opened dialog. Must only be
  // called if `IsShowing()` returns true.
  static void Resolve(bool checked, bool accept);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_FILE_HANDLING_PERMISSION_REQUEST_DIALOG_TEST_API_H_
