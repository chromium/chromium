// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSION_BUBBLE_FILE_HANDLING_PERMISSION_REQUEST_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSION_BUBBLE_FILE_HANDLING_PERMISSION_REQUEST_DIALOG_H_

#include <string>

#include "base/callback.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/permission_prompt.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"

namespace base {
class FilePath;
}

namespace content {
class WebContents;
}

namespace views {
class Checkbox;
}

namespace web_app {
class FileHandlingPermissionRequestDialogTestApi;
}

// A window modal dialog that displays over a PWA window or browser tab when the
// user's ContentSettingsType::FILE_HANDLING permission is set to
// CONTENT_SETTING_ASK, and the PWA is being launched in response to opening a
// file from the system file browser.
//
// This dialog plays the same role as PermissionPromptImpl, but has a few
// desired behavioral differences:
//
// * it shows text that is specific to the request, not just the type of request
//   (i.e. the exact files that are being launched)
// * it's modal and centered, as the action is user-initiated and requires an
//   immediate response
// * there's a checkbox to allow toggling permanency of the decision. This
//   checkbox is presented to the user with text akin to "don't ask again" and
//   is checked by default.
// * it doesn't coalesce with other permission requests
class FileHandlingPermissionRequestDialog
    : public views::DialogDelegateView,
      public permissions::PermissionPrompt {
 public:
  METADATA_HEADER(FileHandlingPermissionRequestDialog);

  // The result callback takes two boolean arguments. The first indicates
  // whether the action was allowed, and the second whether the choice was
  // permanent.
  FileHandlingPermissionRequestDialog(
      content::WebContents* web_contents,
      const std::vector<base::FilePath> file_names,
      base::OnceCallback<void(bool, bool)> result_callback);
  FileHandlingPermissionRequestDialog(
      const FileHandlingPermissionRequestDialog&) = delete;
  FileHandlingPermissionRequestDialog& operator=(
      const FileHandlingPermissionRequestDialog&) = delete;
  ~FileHandlingPermissionRequestDialog() override;

  // permissions::PermissionPrompt:
  void UpdateAnchor() override;
  TabSwitchingBehavior GetTabSwitchingBehavior() override;
  permissions::PermissionPromptDisposition GetPromptDisposition()
      const override;

 private:
  friend class web_app::FileHandlingPermissionRequestDialogTestApi;

  // Returns the last created instance, for use in tests only.
  static FileHandlingPermissionRequestDialog* GetInstanceForTesting();

  void OnDialogAccepted();
  void OnDialogCanceled();

  views::Checkbox* checkbox_ = nullptr;

  base::OnceCallback<void(bool, bool)> result_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSION_BUBBLE_FILE_HANDLING_PERMISSION_REQUEST_DIALOG_H_
