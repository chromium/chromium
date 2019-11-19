// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/auto_signin_first_run_dialog_view.h"

#include "build/build_config.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/passwords/credential_manager_dialog_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

AutoSigninFirstRunDialogView::AutoSigninFirstRunDialogView(
    CredentialManagerDialogController* controller,
    content::WebContents* web_contents)
    : controller_(controller), web_contents_(web_contents) {
  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_AUTO_SIGNIN_FIRST_RUN_OK));
  DialogDelegate::set_button_label(ui::DIALOG_BUTTON_CANCEL,
                                   l10n_util::GetStringUTF16(IDS_TURN_OFF));
  chrome::RecordDialogCreation(chrome::DialogIdentifier::AUTO_SIGNIN_FIRST_RUN);
}

AutoSigninFirstRunDialogView::~AutoSigninFirstRunDialogView() {
}

void AutoSigninFirstRunDialogView::ShowAutoSigninPrompt() {
  InitWindow();
  constrained_window::ShowWebModalDialogViews(this, web_contents_);
}

void AutoSigninFirstRunDialogView::ControllerGone() {
  // During Widget::Close() phase some accessibility event may occur. Thus,
  // |controller_| should be kept around.
  GetWidget()->Close();
  controller_ = nullptr;
}

ui::ModalType AutoSigninFirstRunDialogView::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

base::string16 AutoSigninFirstRunDialogView::GetWindowTitle() const {
  return controller_->GetAutoSigninPromoTitle();
}

bool AutoSigninFirstRunDialogView::ShouldShowCloseButton() const {
  return false;
}

gfx::Size AutoSigninFirstRunDialogView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

void AutoSigninFirstRunDialogView::WindowClosing() {
  if (controller_)
    controller_->OnCloseDialog();
}

bool AutoSigninFirstRunDialogView::Cancel() {
  // On Mac the button click event may be dispatched after the dialog was
  // hidden. Thus, the controller can be NULL.
  if (controller_)
    controller_->OnAutoSigninTurnOff();
  return true;
}

bool AutoSigninFirstRunDialogView::Accept() {
  // On Mac the button click event may be dispatched after the dialog was
  // hidden. Thus, the controller can be NULL.
  if (controller_)
    controller_->OnAutoSigninOK();
  return true;
}

bool AutoSigninFirstRunDialogView::Close() {
  // Do nothing rather than running Cancel(), which would turn off auto-signin.
  return true;
}

void AutoSigninFirstRunDialogView::InitWindow() {
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::TEXT, views::TEXT));
  SetLayoutManager(std::make_unique<views::FillLayout>());

  auto label = std::make_unique<views::Label>(controller_->GetAutoSigninText(),
                                              CONTEXT_BODY_TEXT_LARGE,
                                              views::style::STYLE_SECONDARY);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(label.release());
}

AutoSigninFirstRunPrompt* CreateAutoSigninPromptView(
    CredentialManagerDialogController* controller,
    content::WebContents* web_contents) {
  return new AutoSigninFirstRunDialogView(controller, web_contents);
}
