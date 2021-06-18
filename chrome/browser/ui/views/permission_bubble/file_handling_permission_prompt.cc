// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permission_bubble/file_handling_permission_prompt.h"

#include "base/bind.h"
#include "chrome/browser/ui/views/permission_bubble/file_handling_permission_request_dialog.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_launch/web_launch_files_helper.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/permissions/permission_uma_util.h"
#include "content/public/browser/web_contents.h"

FileHandlingPermissionPrompt::~FileHandlingPermissionPrompt() {
  if (widget_)
    widget_->Close();
}

// static
std::unique_ptr<FileHandlingPermissionPrompt>
FileHandlingPermissionPrompt::Create(content::WebContents* web_contents,
                                     Delegate* delegate) {
  auto* web_launch_files_helper =
      web_launch::WebLaunchFilesHelper::GetForWebContents(web_contents);
  if (!web_launch_files_helper)
    return nullptr;

  return base::WrapUnique(new FileHandlingPermissionPrompt(
      web_contents, delegate, web_launch_files_helper->launch_paths()));
}

FileHandlingPermissionPrompt::FileHandlingPermissionPrompt(
    content::WebContents* web_contents,
    Delegate* delegate,
    const std::vector<base::FilePath>& launch_paths)
    : delegate_(delegate) {
  auto dialog = std::make_unique<FileHandlingPermissionRequestDialog>(
      web_contents, launch_paths,
      base::BindOnce(&FileHandlingPermissionPrompt::OnDialogResponse,
                     weak_ptr_factory_.GetWeakPtr()));
  widget_ = constrained_window::ShowWebModalDialogViews(dialog.release(),
                                                        web_contents);
  widget_->Show();
}

void FileHandlingPermissionPrompt::UpdateAnchor() {}

permissions::PermissionPrompt::TabSwitchingBehavior
FileHandlingPermissionPrompt::GetTabSwitchingBehavior() {
  return kKeepPromptAlive;
}

permissions::PermissionPromptDisposition
FileHandlingPermissionPrompt::GetPromptDisposition() const {
  return permissions::PermissionPromptDisposition::MODAL_DIALOG;
}

void FileHandlingPermissionPrompt::OnDialogResponse(bool allow,
                                                    bool permanently) {
  widget_ = nullptr;
  if (allow && permanently) {
    delegate_->Accept();
  } else if (allow && !permanently) {
    delegate_->AcceptThisTime();
  } else if (!allow && permanently) {
    delegate_->Deny();
  } else {
    // TODO(estade): we'll land here if the user unchecks the permanency
    // checkbox and disallows the file launch. This is currently treated as a
    // dismissal. After enough dismissals, the permission will be embargoed.
    // It would probably better not to embargo, since the user must have
    // explicitly opted out of making the decision permanent.
    delegate_->Closing();
  }
}
