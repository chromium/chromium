// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feedback/feedback_dialog.h"

#include <memory>
#include <utility>

#include "base/strings/string16.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "content/public/browser/browser_thread.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace {

// Default width/height of the Feedback Window.
const int kDefaultWidth = 500;
const int kDefaultHeight = 628;

}  // namespace

using extensions::api::feedback_private::FeedbackInfo;

// static
FeedbackDialog* FeedbackDialog::current_instance_ = nullptr;

// static
void FeedbackDialog::CreateOrShow(const FeedbackInfo& info) {
  // Focus the window hosting the dialog that has already been created.
  if (current_instance_) {
    DCHECK(current_instance_->widget_);
    current_instance_->widget_->Show();
    return;
  }

  current_instance_ = new FeedbackDialog(info);
  gfx::NativeWindow window = chrome::ShowWebDialog(
      nullptr /* parent */, ProfileManager::GetActiveUserProfile(),
      current_instance_);
  current_instance_->widget_ = views::Widget::GetWidgetForNativeWindow(window);
}

FeedbackDialog::FeedbackDialog(const FeedbackInfo& info) : widget_(nullptr) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  set_can_resize(false);
  set_can_minimize(true);
}

FeedbackDialog::~FeedbackDialog() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  current_instance_ = nullptr;
}

ui::ModalType FeedbackDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_NONE;
}

std::u16string FeedbackDialog::GetDialogTitle() const {
  return std::u16string();
}

GURL FeedbackDialog::GetDialogContentURL() const {
  return GURL("chrome://feedback");
}

void FeedbackDialog::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {}

void FeedbackDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDefaultWidth, kDefaultHeight);
}

std::string FeedbackDialog::GetDialogArgs() const {
  return std::string();
}

void FeedbackDialog::OnDialogClosed(const std::string& json_retval) {
  DCHECK(this == current_instance_);
  delete this;
}

void FeedbackDialog::OnCloseContents(WebContents* source,
                                     bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool FeedbackDialog::ShouldShowDialogTitle() const {
  return false;
}

bool FeedbackDialog::ShouldShowCloseButton() const {
  return false;
}

ui::WebDialogDelegate::FrameKind FeedbackDialog::GetWebDialogFrameKind() const {
  return ui::WebDialogDelegate::FrameKind::kDialog;
}
