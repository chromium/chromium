// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LOGIN_LOGIN_SCREEN_EXTENSION_UI_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_LOGIN_LOGIN_SCREEN_EXTENSION_UI_DIALOG_DELEGATE_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class Size;
}  // namespace gfx

namespace ash {
namespace login_screen_extension_ui {
struct CreateOptions;

// This class is used to provide data from a chrome.loginScreenUi API call to
// the WebDialog.
class DialogDelegate : public ui::WebDialogDelegate {
 public:
  explicit DialogDelegate(CreateOptions* create_options);

  DialogDelegate(const DialogDelegate&) = delete;
  DialogDelegate& operator=(const DialogDelegate&) = delete;

  ~DialogDelegate() override;

  void set_can_close(bool can_close) { can_close_ = can_close; }
  void set_native_window(gfx::NativeWindow native_window) {
    native_window_ = native_window;
  }

  // ui::WebDialogDelegate:
  ui::mojom::ModalType GetDialogModalType() const override;
  std::u16string GetDialogTitle() const override;
  GURL GetDialogContentURL() const override;
  void GetDialogSize(gfx::Size* size) const override;
  bool OnDialogCloseRequested() override;
  std::string GetDialogArgs() const override;
  // NOTE: This function deletes this object at the end.
  void OnDialogClosed(const std::string& json_retval) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  bool ShouldCloseDialogOnEscape() const override;
  bool ShouldShowDialogTitle() const override;
  bool ShouldCenterDialogTitleText() const override;
  bool ShouldShowCloseButton() const override;

 private:
  const std::string extension_name_;
  const GURL content_url_;
  bool can_close_ = false;

  base::OnceClosure close_callback_;

  gfx::NativeWindow native_window_ = gfx::NativeWindow();
};

}  // namespace login_screen_extension_ui
}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_LOGIN_LOGIN_SCREEN_EXTENSION_UI_DIALOG_DELEGATE_H_
