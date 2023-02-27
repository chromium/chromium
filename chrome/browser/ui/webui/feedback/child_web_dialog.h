// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEEDBACK_CHILD_WEB_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_FEEDBACK_CHILD_WEB_DIALOG_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "url/gurl.h"

class Profile;
namespace views {
class Widget;
}

// Launches a child web dialog with specified URL and title.
class ChildWebDialog : public ui::WebDialogDelegate {
 public:
  ChildWebDialog(Profile* profile,
                 views::Widget* parent_widget,
                 const GURL& url,
                 const std::u16string& title,
                 ui::ModalType modal_type = ui::MODAL_TYPE_WINDOW,
                 const std::string& args = "",
                 int dialog_width = 640,
                 int dialog_height = 400,
                 bool can_resize = true,
                 bool can_minimize = false);
  ChildWebDialog(const ChildWebDialog&) = delete;
  ChildWebDialog& operator=(const ChildWebDialog&) = delete;
  ~ChildWebDialog() override;

  void Show();

 protected:
  // ui::WebDialogDelegate implementation.
  bool CanMaximizeDialog() const override;
  std::string GetDialogArgs() const override;
  GURL GetDialogContentURL() const override;
  ui::ModalType GetDialogModalType() const override;
  void GetDialogSize(gfx::Size* size) const override;
  std::u16string GetDialogTitle() const override;
  void GetMinimumDialogSize(gfx::Size* size) const override;
  void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override;
  // NOTE: This function deletes this object at the end.
  void OnDialogClosed(const std::string& json_retval) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  bool ShouldShowDialogTitle() const override;

 private:
  const raw_ptr<Profile> profile_;
  const raw_ptr<views::Widget> parent_widget_;
  const std::u16string title_;
  const GURL url_;
  const ui::ModalType modal_type_;
  const std::string args_;
  const int dialog_width_;
  const int dialog_height_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_FEEDBACK_CHILD_WEB_DIALOG_H_
