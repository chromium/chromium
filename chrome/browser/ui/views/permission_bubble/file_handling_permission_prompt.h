// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSION_BUBBLE_FILE_HANDLING_PERMISSION_PROMPT_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSION_BUBBLE_FILE_HANDLING_PERMISSION_PROMPT_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/permission_prompt.h"

namespace base {
class FilePath;
}

namespace content {
class WebContents;
}

namespace views {
class Widget;
}

// A specialized prompt specifically that uses
// `FileHandlingPermissionRequestDialog` for File Handling permission requests.
class FileHandlingPermissionPrompt : public permissions::PermissionPrompt {
 public:
  FileHandlingPermissionPrompt(const FileHandlingPermissionPrompt&) = delete;
  FileHandlingPermissionPrompt& operator=(const FileHandlingPermissionPrompt&) =
      delete;
  ~FileHandlingPermissionPrompt() override;

  // A factory function which can fail and return nullptr.
  static std::unique_ptr<FileHandlingPermissionPrompt> Create(
      content::WebContents* web_contents,
      Delegate* delegate);

  // permissions::PermissionPrompt:
  void UpdateAnchor() override;
  TabSwitchingBehavior GetTabSwitchingBehavior() override;
  permissions::PermissionPromptDisposition GetPromptDisposition()
      const override;

 private:
  FileHandlingPermissionPrompt(content::WebContents* web_contents,
                               Delegate* delegate,
                               const std::vector<base::FilePath>& launch_paths);

  void OnDialogResponse(bool allow, bool permanently);

  Delegate* delegate_;
  views::Widget* widget_ = nullptr;
  base::WeakPtrFactory<FileHandlingPermissionPrompt> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSION_BUBBLE_FILE_HANDLING_PERMISSION_PROMPT_H_
