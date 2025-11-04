// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SYSTEM_WEB_DIALOG_SYSTEM_WEB_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SYSTEM_WEB_DIALOG_SYSTEM_WEB_DIALOG_DELEGATE_H_

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_ui.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "url/gurl.h"

// WebDialogDelegate for system Web UI dialogs, e.g. dialogs opened from the
// Ash system tray. These dialogs are normally movable and draggable so that
// content from other pages can be copy-pasted, but kept always-on-top so that
// they do not get lost behind other windows. On screens that use an overlay
// like the login and lock screens, the dialog must be modal to be displayed on
// top of the overlay.

namespace ash {

class SystemWebDialogDelegate : public ui::WebDialogDelegate {
 public:
  // Default margin (in pixels) used when sizing a dialog to an internal screen;
  // see ComputeDialogSizeForInternalScreen().
  static const size_t kDialogMarginForInternalScreenPx;

  // Returns the instance whose Id() matches |id|. If more than one instance
  // matches, the first matching instance created is returned.
  static SystemWebDialogDelegate* FindInstance(const std::string& id);

  // Returns true if there is a system dialog with |url| loaded.
  static bool HasInstance(const GURL& url);

  // Generates a dialog size which fits within the device's internal screen. If
  // possible, this function simply returns |preferred_size|, but if that size
  // does not fit within the screen's bounds with a margin of
  // |kDialogMarginForInternalScreenPx| pixels on all sides, a smaller size is
  // returned.
  static gfx::Size ComputeDialogSizeForInternalScreen(
      const gfx::Size& preferred_size);

  // |gurl| is the HTML file path for the dialog content and must be set.
  // |title| may be empty in which case the dialog title is not shown.
  SystemWebDialogDelegate(const GURL& gurl, const std::u16string& title);

  SystemWebDialogDelegate(const SystemWebDialogDelegate&) = delete;
  SystemWebDialogDelegate& operator=(const SystemWebDialogDelegate&) = delete;

  ~SystemWebDialogDelegate() override;

  // Returns an identifier used for matching an instance in FindInstance.
  // By default returns gurl_.spec() which should be sufficient for dialogs
  // that only support a single instance.
  virtual std::string Id();

  // Adjust the init params for the widget. By default makes no change.
  virtual void AdjustWidgetInitParams(views::Widget::InitParams* params) {}

  // Brings the dialog window to the front.
  void StackAtTop();

  // Focuses the dialog window. Note: No-op for modal dialogs, see
  // implementation for details.
  void Focus();

  // Closes the dialog window.
  void Close();

  // ui::WebDialogDelegate
  // Derived classes that override this method should still call
  // SystemWebDialogDelegate::OnDialogShown.
  void OnDialogShown(content::WebUI* webui) override;

  // Shows a system dialog using the specified BrowserContext (or Profile).
  // If |parent| is not null, the dialog will be parented to |parent|.
  // Otherwise it will be attached to either the AlwaysOnTop container or the
  // LockSystemModal container, depending on the session state at creation.
  // TODO(https://crbug.com/1268547): Passing a non-null |parent| here or to
  // ShowSystemDialog() seems to prevent the dialog from properly repositioning
  // on screen size changes (i.e. when the docked screen magnifier is enabled).
  void ShowSystemDialogForBrowserContext(
      content::BrowserContext* context,
      gfx::NativeWindow parent = gfx::NativeWindow());
  // Same as previous but shows a system dialog using the current active
  // profile.
  void ShowSystemDialog(gfx::NativeWindow parent = gfx::NativeWindow());

  content::WebUI* GetWebUIForTest() { return webui_; }

  // Width is consistent with the Settings UI.
  static constexpr int kDialogWidth = 512;
  static constexpr int kDialogHeight = 480;

 protected:
  FRIEND_TEST_ALL_PREFIXES(SystemWebDialogLoginTest, NonModalTest);
  FRIEND_TEST_ALL_PREFIXES(SystemWebDialogTest, StackAtTop);
  FRIEND_TEST_ALL_PREFIXES(SystemWebDialogTest, ShowBeforeFocus);

  // Returns the dialog window (pointer to |aura::Window|). This will be a
  // |nullptr| if the dialog has not been created yet.
  gfx::NativeWindow dialog_window() const { return dialog_window_; }

  content::WebUI* webui() { return webui_; }

 private:
  raw_ptr<content::WebUI, DanglingUntriaged> webui_ = nullptr;
  gfx::NativeWindow dialog_window_ = gfx::NativeWindow();
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SYSTEM_WEB_DIALOG_SYSTEM_WEB_DIALOG_DELEGATE_H_
