// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_SYSTEM_WEB_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_SYSTEM_WEB_DIALOG_DELEGATE_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "url/gurl.h"

// ui::WebDialogDelegate for always-on-top system Web UI dialogs, e.g. dialogs
// opened from the ash system tray. These dialogs are intentionally movable /
// draggable so that content from other pages can be copy-pasted.

namespace chromeos {

class SystemWebDialogDelegate : public ui::WebDialogDelegate {
 public:
  // |gurl| is the HTML file path for the dialog content and must be set.
  // |title| may be empty in which case ShouldShowDialogTitle() returns false.
  SystemWebDialogDelegate(const GURL& gurl, const base::string16& title);
  ~SystemWebDialogDelegate() override;

  // ui::WebDialogDelegate
  ui::ModalType GetDialogModalType() const override;
  base::string16 GetDialogTitle() const override;
  GURL GetDialogContentURL() const override;
  void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override;
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;
  void OnDialogShown(content::WebUI* webui,
                     content::RenderViewHost* render_view_host) override;
  // Note: deletes |this|.
  void OnDialogClosed(const std::string& json_retval) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  bool ShouldShowDialogTitle() const override;

  // Show the dialog using the current ative profile and the proper ash
  // shell container.
  // |is_minimal_style| means whether title area of the dialog should be hide.
  void ShowSystemDialog(bool is_minimal_style = false);

  content::WebUI* GetWebUIForTest() { return webui_; }

  // Width is consistent with the Settings UI.
  static constexpr int kDialogWidth = 512;
  static constexpr int kDialogHeight = 480;

 protected:
  gfx::NativeWindow dialog_window() const { return dialog_window_; }

 private:
  GURL gurl_;
  base::string16 title_;
  content::WebUI* webui_ = nullptr;
  ui::ModalType modal_type_;
  gfx::NativeWindow dialog_window_;

  DISALLOW_COPY_AND_ASSIGN(SystemWebDialogDelegate);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_SYSTEM_WEB_DIALOG_DELEGATE_H_
