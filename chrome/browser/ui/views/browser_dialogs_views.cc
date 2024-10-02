// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_dialogs.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/extensions/api/chrome_device_permissions_prompt.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_editor_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/task_manager_view.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/permissions/chooser_controller.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/interaction/element_tracker_views.h"

// This file provides definitions of desktop browser dialog-creation methods for
// all toolkit-views platforms.

// static
void BookmarkEditor::Show(gfx::NativeWindow parent_window,
                          Profile* profile,
                          const EditDetails& details,
                          Configuration configuration,
                          OnSaveCallback on_save_callback) {
  auto editor = std::make_unique<BookmarkEditorView>(
      profile, details, configuration, std::move(on_save_callback));
  editor->Show(parent_window);
  editor.release();  // BookmarkEditorView is self-deleting
}

void ChromeDevicePermissionsPrompt::ShowDialog() {
  ShowDialogViews();
}

namespace chrome {

#if !BUILDFLAG(IS_MAC)
task_manager::TaskManagerTableModel* ShowTaskManager(Browser* browser) {
  return task_manager::TaskManagerView::Show(browser);
}

void HideTaskManager() {
  task_manager::TaskManagerView::Hide();
}
#endif

views::Widget* ShowBrowserModal(Browser* browser,
                                std::unique_ptr<ui::DialogModel> dialog_model) {
  return constrained_window::ShowBrowserModal(
      std::move(dialog_model), browser->window()->GetNativeWindow());
}

// TODO(pbos): Move bubble showing out of this file (like ShowBrowserModal) so
// that this code can be used for showing bubbles outside Browser too.
void ShowBubble(Browser* browser,
                ui::ElementIdentifier anchor_element_id,
                std::unique_ptr<ui::DialogModel> dialog_model) {
  views::View* const anchor_view =
      views::ElementTrackerViews::GetInstance()->GetUniqueView(
          anchor_element_id,
          views::ElementTrackerViews::GetContextForView(
              BrowserView::GetBrowserViewForBrowser(browser)));
  DCHECK(anchor_view);
  // TODO(pbos): Add a version of BubbleBorder::Arrow that infers position
  // automatically based on the anchor's position relative to its widget.
  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      std::move(dialog_model), anchor_view, views::BubbleBorder::TOP_RIGHT);
  views::BubbleDialogDelegate::CreateBubble(std::move(bubble))->Show();
}

}  // namespace chrome
