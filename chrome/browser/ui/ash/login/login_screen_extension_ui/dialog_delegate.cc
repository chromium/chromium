// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/login/login_screen_extension_ui/dialog_delegate.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/ash/login/login_screen_extension_ui/create_options.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"

namespace ash {
namespace {

const double kRelativeScreenWidth = 0.9;
const double kRelativeScreenHeight = 0.8;

}  // namespace

namespace login_screen_extension_ui {

DialogDelegate::DialogDelegate(CreateOptions* create_options)
    : extension_name_(create_options->extension_name),
      content_url_(create_options->content_url),
      can_close_(create_options->can_be_closed_by_user),
      close_callback_(std::move(create_options->close_callback)) {
  set_can_resize(false);
}

DialogDelegate::~DialogDelegate() = default;

ui::mojom::ModalType DialogDelegate::GetDialogModalType() const {
  return ui::mojom::ModalType::kWindow;
}

std::u16string DialogDelegate::GetDialogTitle() const {
  return l10n_util::GetStringFUTF16(IDS_LOGIN_EXTENSION_UI_DIALOG_TITLE,
                                    base::UTF8ToUTF16(extension_name_));
}

GURL DialogDelegate::GetDialogContentURL() const {
  return content_url_;
}

void DialogDelegate::GetDialogSize(gfx::Size* size) const {
  gfx::Size screen_size = display::Screen::GetScreen()
                              ->GetDisplayNearestWindow(native_window_)
                              .size();
  *size = gfx::Size(kRelativeScreenWidth * screen_size.width(),
                    kRelativeScreenHeight * screen_size.height());
}

bool DialogDelegate::OnDialogCloseRequested() {
  return can_close_;
}

std::string DialogDelegate::GetDialogArgs() const {
  return std::string();
}

void DialogDelegate::OnDialogClosed(const std::string& json_retval) {
  std::move(close_callback_).Run();
  delete this;
}

void DialogDelegate::OnCloseContents(content::WebContents* source,
                                     bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool DialogDelegate::ShouldCloseDialogOnEscape() const {
  return false;
}

bool DialogDelegate::ShouldShowDialogTitle() const {
  return true;
}

bool DialogDelegate::ShouldCenterDialogTitleText() const {
  return true;
}

bool DialogDelegate::ShouldShowCloseButton() const {
  return can_close_;
}

}  // namespace login_screen_extension_ui
}  // namespace ash
