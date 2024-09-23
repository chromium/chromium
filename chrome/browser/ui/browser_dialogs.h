// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_DIALOGS_H_
#define CHROME_BROWSER_UI_BROWSER_DIALOGS_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/compose/buildflags.h"
#include "components/compose/core/browser/compose_client.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/dialog_model.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/native_widget_types.h"

class Browser;
class Profile;

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace extensions {
class Extension;
}

namespace permissions {
class ChooserController;
enum class PermissionAction;
}  // namespace permissions

namespace task_manager {
class TaskManagerTableModel;
}

namespace ui {
class WebDialogDelegate;
struct SelectedFileInfo;
}  // namespace ui

namespace views {
class Widget;
}  // namespace views

#if BUILDFLAG(ENABLE_COMPOSE)
namespace compose {
class ComposeDialogController;
}  // namespace compose
#endif

namespace chrome {

// Shows or hides the Task Manager. |browser| can be NULL when called from Ash.
// Returns a pointer to the underlying TableModel, which can be ignored, or used
// for testing.
task_manager::TaskManagerTableModel* ShowTaskManager(Browser* browser);
void HideTaskManager();

// Creates and shows an HTML dialog with the given delegate and context.
// The window is automatically destroyed when it is closed.
// Returns the created window.
//
// Make sure to use the returned window only when you know it is safe
// to do so, i.e. before OnDialogClosed() is called on the delegate.
gfx::NativeWindow ShowWebDialog(gfx::NativeView parent,
                                content::BrowserContext* context,
                                ui::WebDialogDelegate* delegate,
                                bool show = true);

// Show `dialog_model` as a modal dialog to `browser`.
views::Widget* ShowBrowserModal(Browser* browser,
                                std::unique_ptr<ui::DialogModel> dialog_model);

// Show `dialog_model` as a bubble anchored to `anchor_element` in `browser`.
// `anchor_element` must refer to an element currently present in `browser`.
// TODO(pbos): Make utility functions for querying whether an anchor_element is
// present in `browser` or `browser_window` and then refer to those here so that
// a call site can provide fallback options for `anchor_element`.
void ShowBubble(Browser* browser,
                ui::ElementIdentifier anchor_element,
                std::unique_ptr<ui::DialogModel> dialog_model);

// Shows the create chrome app shortcut dialog box.
// |close_callback| may be null.
void ShowCreateChromeAppShortcutsDialog(
    gfx::NativeWindow parent_window,
    Profile* profile,
    const extensions::Extension* app,
    base::OnceCallback<void(bool /* created */)> close_callback);

// Shows the create chrome app shortcut dialog box. Same as above but for a
// WebApp instead of an Extension. |close_callback| may be null.
void ShowCreateChromeAppShortcutsDialog(
    gfx::NativeWindow parent_window,
    Profile* profile,
    const std::string& web_app_id,
    base::OnceCallback<void(bool /* created */)> close_callback);

// Shows a tab modal dialog based on `dialog_model`.
void ShowTabModal(std::unique_ptr<ui::DialogModel> dialog_model,
                  content::WebContents* web_contents);

#if BUILDFLAG(IS_MAC)

// Bridging methods that show/hide the toolkit-views based Task Manager on Mac.
task_manager::TaskManagerTableModel* ShowTaskManagerViews(Browser* browser);
void HideTaskManagerViews();

#endif  // BUILDFLAG(IS_MAC)

// Returns a OnceClosure that client code can call to close the device chooser.
// This OnceClosure references the actual dialog as a WeakPtr, so it's safe to
// call at any point.
#if defined(TOOLKIT_VIEWS)
base::OnceClosure ShowDeviceChooserDialog(
    content::RenderFrameHost* owner,
    std::unique_ptr<permissions::ChooserController> controller);
bool IsDeviceChooserShowingForTesting(Browser* browser);
#endif

// Show the prompt to set a window name for browser's window, optionally with
// the given context.
void ShowWindowNamePrompt(Browser* browser);
std::unique_ptr<ui::DialogModel> CreateWindowNamePromptDialogModelForTesting(
    Browser* browser);

#if BUILDFLAG(ENABLE_COMPOSE)
std::unique_ptr<compose::ComposeDialogController> ShowComposeDialog(
    content::WebContents& web_contents,
    const gfx::RectF& element_bounds_in_screen,
    compose::ComposeClient::FieldIdentifier field_ids);
#endif

// Shows the 'Create Shortcut' dialog to create fire and forget entities on the
// desktop of the OS. Before the dialog is shown, the necessary metadata is
// gathered from the browser's active WebContents.
// Triggered from the three-dot menu on Chrome, Save & Share > Create Shortcut.
void CreateDesktopShortcutForActiveWebContents(Browser* browser);

}  // namespace chrome

void ShowFolderUploadConfirmationDialog(
    const base::FilePath& path,
    base::OnceCallback<void(const std::vector<ui::SelectedFileInfo>&)> callback,
    std::vector<ui::SelectedFileInfo> selected_files,
    content::WebContents* web_contents);

#endif  // CHROME_BROWSER_UI_BROWSER_DIALOGS_H_
