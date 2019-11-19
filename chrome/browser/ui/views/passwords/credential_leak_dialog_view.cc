// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/credential_leak_dialog_view.h"

#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/passwords/credential_leak_dialog_controller.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/theme_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"

using views::BoxLayout;

namespace {

// Fixed height of the illustration shown on the top of the dialog.
constexpr int kIllustrationHeight = 148;

// Fixed background color of the illustration shown on the top of the dialog in
// normal mode.
constexpr SkColor kPictureBackgroundColor = SkColorSetARGB(0x0A, 0, 0, 0);

// Fixed background color of the illustration shown on the top of the dialog in
// dark mode.
constexpr SkColor kPictureBackgroundColorDarkMode =
    SkColorSetARGB(0x1A, 0x00, 0x00, 0x00);

// Updates the image displayed on the illustration based on the current theme.
void UpdateImageView(NonAccessibleImageView* image_view,
                     bool dark_mode_enabled) {
  image_view->SetImage(
      gfx::CreateVectorIcon(dark_mode_enabled ? kPasswordCheckWarningDarkIcon
                                              : kPasswordCheckWarningIcon,
                            dark_mode_enabled ? kPictureBackgroundColorDarkMode
                                              : kPictureBackgroundColor));
}

// Creates the illustration which is rendered on top of the dialog.
std::unique_ptr<NonAccessibleImageView> CreateIllustration(
    bool dark_mode_enabled) {
  const gfx::Size illustration_size(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH),
      kIllustrationHeight);
  auto image_view = std::make_unique<NonAccessibleImageView>();
  image_view->SetPreferredSize(illustration_size);
  UpdateImageView(image_view.get(), dark_mode_enabled);
  image_view->SetSize(illustration_size);
  image_view->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  return image_view;
}

// Creates the content containing the title and description for the dialog
// rendered below the illustration.
std::unique_ptr<views::View> CreateContent(const base::string16& title,
                                           const base::string16& description) {
  auto content = std::make_unique<views::View>();
  content->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kVertical, gfx::Insets(),
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  content->SetBorder(views::CreateEmptyBorder(
      views::LayoutProvider::Get()->GetDialogInsetsForContentType(
          views::CONTROL, views::CONTROL)));

  auto title_label = std::make_unique<views::Label>(
      title, views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_PRIMARY);
  title_label->SetMultiLine(true);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  content->AddChildView(std::move(title_label));

  auto description_label =
      std::make_unique<views::StyledLabel>(description, nullptr);
  content->AddChildView(std::move(description_label));

  return content;
}

}  // namespace

CredentialLeakDialogView::CredentialLeakDialogView(
    CredentialLeakDialogController* controller,
    content::WebContents* web_contents)
    : controller_(controller), web_contents_(web_contents) {
  DCHECK(controller);
  DCHECK(web_contents);

  DialogDelegate::set_button_label(ui::DIALOG_BUTTON_OK,
                                   controller_->GetAcceptButtonLabel());
  DialogDelegate::set_button_label(ui::DIALOG_BUTTON_CANCEL,
                                   controller_->GetCancelButtonLabel());
}

CredentialLeakDialogView::~CredentialLeakDialogView() = default;

void CredentialLeakDialogView::ShowCredentialLeakPrompt() {
  InitWindow();
  constrained_window::ShowWebModalDialogViews(this, web_contents_);
}

void CredentialLeakDialogView::ControllerGone() {
  // Widget::Close() synchronously calls Close() on this instance, which resets
  // the |controller_|.
  GetWidget()->Close();
}

ui::ModalType CredentialLeakDialogView::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

gfx::Size CredentialLeakDialogView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

bool CredentialLeakDialogView::Cancel() {
  if (controller_)
    // Since OnCancelDialog() synchronously invokes Close() on this instance, we
    // need to clear the |controller_| before to avoid notifying the controller
    // twice.
    std::exchange(controller_, nullptr)->OnCancelDialog();
  return true;
}

bool CredentialLeakDialogView::Accept() {
  if (controller_)
    // Since OnAcceptDialog() synchronously invokes Close() on this instance, we
    // need to clear the |controller_| before to avoid notifying the controller
    // twice.
    std::exchange(controller_, nullptr)->OnAcceptDialog();
  return true;
}

bool CredentialLeakDialogView::Close() {
  if (controller_)
    // Since OnCloseDialog() synchronously invokes Close() on this instance, we
    // need to clear the |controller_| before to avoid notifying the controller
    // twice.
    std::exchange(controller_, nullptr)->OnCloseDialog();
  return true;
}

int CredentialLeakDialogView::GetDialogButtons() const {
  return controller_->ShouldShowCancelButton()
             ? ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL
             : ui::DIALOG_BUTTON_OK;
}

bool CredentialLeakDialogView::ShouldShowCloseButton() const {
  return false;
}

void CredentialLeakDialogView::OnThemeChanged() {
  UpdateImageView(image_view_, GetNativeTheme()->ShouldUseDarkColors());
}

void CredentialLeakDialogView::InitWindow() {
  SetLayoutManager(std::make_unique<BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      0 /* between_child_spacing */));
  std::unique_ptr<NonAccessibleImageView> illustration =
      CreateIllustration(GetNativeTheme()->ShouldUseDarkColors());
  image_view_ = illustration.get();
  std::unique_ptr<views::View> content =
      CreateContent(controller_->GetTitle(), controller_->GetDescription());
  AddChildView(std::move(illustration));
  AddChildView(std::move(content));
}

CredentialLeakPrompt* CreateCredentialLeakPromptView(
    CredentialLeakDialogController* controller,
    content::WebContents* web_contents) {
  return new CredentialLeakDialogView(controller, web_contents);
}
