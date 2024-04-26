// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_RESTORE_PERMISSION_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_RESTORE_PERMISSION_BUBBLE_VIEW_H_

#include "chrome/browser/file_system_access/file_system_access_permission_request_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_header_macros.h"

// TODO(crbug.com/40101962): Re-define this temporary value in layout provider
// once the spec is ready. Make the style GM3-compatible.
constexpr int DISTANCE_BUTTON_VERTICAL = 8;

// Bubble dialog that prompts user to restore the permission for
// files/directories previously granted to.
//
// TODO(crbug.com/40101962): This UI is still in progress and missing correct
// styles, accessibility support, etc.
class FileSystemAccessRestorePermissionBubbleView
    : public LocationBarBubbleDelegateView {
  METADATA_HEADER(FileSystemAccessRestorePermissionBubbleView,
                  LocationBarBubbleDelegateView)

 public:
  FileSystemAccessRestorePermissionBubbleView(
      const std::u16string window_title,
      const std::vector<
          FileSystemAccessPermissionRequestManager::FileRequestData>& file_data,
      base::OnceCallback<void(permissions::PermissionAction)> callback,
      views::View* anchor_view,
      content::WebContents* web_content);
  FileSystemAccessRestorePermissionBubbleView(
      const FileSystemAccessRestorePermissionBubbleView&) = delete;
  FileSystemAccessRestorePermissionBubbleView& operator=(
      const FileSystemAccessRestorePermissionBubbleView&) = delete;
  ~FileSystemAccessRestorePermissionBubbleView() override;

  static FileSystemAccessRestorePermissionBubbleView* CreateAndShow(
      const FileSystemAccessPermissionRequestManager::RequestData& request,
      base::OnceCallback<void(permissions::PermissionAction)> callback,
      content::WebContents* web_contents);

  // views::BubbleDialogDelegateView:
  void AddedToWidget() override;
  bool ShouldShowCloseButton() const override;
  std::u16string GetWindowTitle() const override;

  enum class RestorePermissionButton {
    kAllowOnce = 0,
    kAllowAlways = 1,
    kDeny = 2,
    kNum = kDeny,
  };
  void OnButtonPressed(RestorePermissionButton button_type);
  void OnPromptDismissed();
  void UpdateAnchor(Browser* browser);

 private:
  const std::u16string window_title_;
  base::OnceCallback<void(permissions::PermissionAction)> callback_;
};

void ShowFileSystemAccessRestorePermissionDialog(
    const FileSystemAccessPermissionRequestManager::RequestData& request,
    base::OnceCallback<void(permissions::PermissionAction)> callback,
    content::WebContents* web_contents);

FileSystemAccessRestorePermissionBubbleView*
GetFileSystemAccessRestorePermissionDialogForTesting(
    const FileSystemAccessPermissionRequestManager::RequestData& request,
    base::OnceCallback<void(permissions::PermissionAction)> callback,
    content::WebContents* web_contents);

#endif  // CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_RESTORE_PERMISSION_BUBBLE_VIEW_H_
