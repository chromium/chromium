// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_dialogs.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/chooser_controller/chooser_controller.h"
#include "chrome/browser/extensions/api/chrome_device_permissions_prompt.h"
#include "chrome/browser/extensions/chrome_extension_chooser_dialog.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_editor_view.h"
#include "chrome/browser/ui/views/task_manager_view.h"

// This file provides definitions of desktop browser dialog-creation methods for
// all toolkit-views platforms.
// static
std::unique_ptr<LoginHandler> LoginHandler::Create(
    const net::AuthChallengeInfo& auth_info,
    content::WebContents* web_contents,
    LoginAuthRequiredCallback auth_required_callback) {
  return chrome::CreateLoginHandlerViews(auth_info, web_contents,
                                         std::move(auth_required_callback));
}

// static
void BookmarkEditor::Show(gfx::NativeWindow parent_window,
                          Profile* profile,
                          const EditDetails& details,
                          Configuration configuration) {
  auto editor = std::make_unique<BookmarkEditorView>(
      profile, details.parent_node, details, configuration);
  editor->Show(parent_window);
  editor.release();  // BookmarkEditorView is self-deleting
}

void ChromeDevicePermissionsPrompt::ShowDialog() {
  ShowDialogViews();
}

void ChromeExtensionChooserDialog::ShowDialog(
    std::unique_ptr<ChooserController> chooser_controller) const {
  ShowDialogImpl(std::move(chooser_controller));
}

namespace chrome {

#if !defined(OS_MACOSX)
task_manager::TaskManagerTableModel* ShowTaskManager(Browser* browser) {
  return task_manager::TaskManagerView::Show(browser);
}

void HideTaskManager() {
  task_manager::TaskManagerView::Hide();
}
#endif

}  // namespace chrome
