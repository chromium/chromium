// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feedback/child_web_dialog.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/widget/widget.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace {
// default minimum size of the child dialog
constexpr gfx::Size kMinSize{400, 120};
}  // namespace

///////////////////////////////////////////////////////////////////////////////
// ChildWebDialog, public:

ChildWebDialog::ChildWebDialog(Profile* profile,
                               views::Widget* parent_widget,
                               const GURL& url,
                               const std::u16string& title,
                               ui::ModalType modal_type,
                               int dialog_width,
                               int dialog_height,
                               bool can_resize,
                               bool can_minimize)
    : profile_(profile),
      parent_widget_(parent_widget),
      title_(title),
      url_(url),
      modal_type_(modal_type),
      dialog_width_(dialog_width),
      dialog_height_(dialog_height) {
  set_can_resize(can_resize);
  set_can_minimize(can_minimize);
}

ChildWebDialog::~ChildWebDialog() = default;

void ChildWebDialog::Show() {
  chrome::ShowWebDialog(parent_widget_->GetNativeView(), profile_, this);
}

///////////////////////////////////////////////////////////////////////////////
// ChildWebDialog, protected:

bool ChildWebDialog::CanMaximizeDialog() const {
  return true;
}

ui::ModalType ChildWebDialog::GetDialogModalType() const {
  return modal_type_;
}

std::u16string ChildWebDialog::GetDialogTitle() const {
  return title_;
}

GURL ChildWebDialog::GetDialogContentURL() const {
  return url_;
}

void ChildWebDialog::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {}

void ChildWebDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(dialog_width_, dialog_height_);
}

void ChildWebDialog::GetMinimumDialogSize(gfx::Size* size) const {
  *size = kMinSize;
}

std::string ChildWebDialog::GetDialogArgs() const {
  return std::string();
}

void ChildWebDialog::OnDialogClosed(const std::string& json_retval) {
  delete this;
}

void ChildWebDialog::OnCloseContents(WebContents* source,
                                     bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool ChildWebDialog::ShouldShowDialogTitle() const {
  return true;
}
