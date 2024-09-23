// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/credential_leak_dialog_view.h"

#include "build/build_config.h"
#include "chrome/browser/ui/passwords/credential_leak_dialog_controller.h"
#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/theme_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/bubble/tooltip_icon.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

namespace {

std::unique_ptr<views::TooltipIcon> CreateInfoIcon() {
  auto explanation_tooltip = std::make_unique<views::TooltipIcon>(
      password_manager::GetLeakDetectionTooltip());
  explanation_tooltip->SetBubbleWidth(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  explanation_tooltip->SetAnchorPointArrow(
      views::BubbleBorder::Arrow::TOP_RIGHT);
  return explanation_tooltip;
}

}  // namespace

CredentialLeakDialogView::CredentialLeakDialogView(
    CredentialLeakDialogController* controller,
    content::WebContents* web_contents)
    : controller_(controller), web_contents_(web_contents) {
  DCHECK(controller);
  DCHECK(web_contents);

  SetButtons(controller->ShouldShowCancelButton()
                 ? static_cast<int>(ui::mojom::DialogButton::kOk) |
                       static_cast<int>(ui::mojom::DialogButton::kCancel)
                 : static_cast<int>(ui::mojom::DialogButton::kOk));
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 controller_->GetAcceptButtonLabel());
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 controller_->GetCancelButtonLabel());

  SetModalType(ui::mojom::ModalType::kChild);
  SetShowCloseButton(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  using ControllerClosureFn = void (CredentialLeakDialogController::*)();
  auto close_callback = [](raw_ptr<CredentialLeakDialogController>* controller,
                           ControllerClosureFn fn) {
    // Null out the controller pointer stored in the parent object, to avoid any
    // further calls to the controller and inhibit recursive closes that would
    // otherwise happen in ControllerGone(), and invoke the provided method on
    // the controller.
    //
    // Note that when this lambda gets bound it closes over &controller_, not
    // controller_ itself!
    (controller->ExtractAsDangling()->*(fn))();
  };

  SetAcceptCallback(
      base::BindOnce(close_callback, base::Unretained(&controller_),
                     &CredentialLeakDialogController::OnAcceptDialog));
  SetCancelCallback(
      base::BindOnce(close_callback, base::Unretained(&controller_),
                     &CredentialLeakDialogController::OnCancelDialog));
  SetCloseCallback(
      base::BindOnce(close_callback, base::Unretained(&controller_),
                     &CredentialLeakDialogController::OnCloseDialog));
}

CredentialLeakDialogView::~CredentialLeakDialogView() {
  if (controller_) {
    std::exchange(controller_, nullptr)->ResetDialog();
  }
}

void CredentialLeakDialogView::ShowCredentialLeakPrompt() {
  InitWindow();
  constrained_window::ShowWebModalDialogViews(this, web_contents_);
}

void CredentialLeakDialogView::ControllerGone() {
  // Widget::Close() synchronously calls Close() on this instance, which resets
  // the |controller_|. The null check for |controller_| here is to avoid
  // reentry into Close() - |controller_| might have been nulled out by the
  // closure callbacks already, in which case the dialog is already closing. See
  // the definition of |close_callback| in the constructor.
  if (controller_) {
    GetWidget()->Close();
  }
}

void CredentialLeakDialogView::AddedToWidget() {
  // Set the header image.
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  auto image_view = std::make_unique<ThemeTrackingNonAccessibleImageView>(
      *bundle.GetImageSkiaNamed(IDR_PASSWORD_CHECK),
      *bundle.GetImageSkiaNamed(IDR_PASSWORD_CHECK_DARK),
      base::BindRepeating(&views::BubbleFrameView::GetBackgroundColor,
                          base::Unretained(GetBubbleFrameView())));

  gfx::Size preferred_size = image_view->GetPreferredSize();
  if (!preferred_size.IsEmpty()) {
    float max_width =
        static_cast<float>(ChromeLayoutProvider::Get()->GetDistanceMetric(
            views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
    // Reduce width by a pixel on each side. This enforces that the banner image
    // is rescaled during the ImageView::OnPaint step. Without the rescaling,
    // the image will display compression artifacts due to the size mismatch.
    // TODO(crbug.com/40745285): Remove once the scaling works automatically.
    max_width -= 2;
    const float scale = max_width / preferred_size.width();
    preferred_size = gfx::ScaleToRoundedSize(preferred_size, scale);
    image_view->SetImageSize(preferred_size);
  }
  image_view->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  GetBubbleFrameView()->SetHeaderView(std::move(image_view));
}

std::u16string CredentialLeakDialogView::GetWindowTitle() const {
  // |controller_| can be nullptr when the framework calls this method after a
  // button click.
  return controller_ ? controller_->GetTitle() : std::u16string();
}

void CredentialLeakDialogView::InitWindow() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetBorder(views::CreateEmptyBorder(
      views::LayoutProvider::Get()->GetDialogInsetsForContentType(
          views::DialogContentType::kControl,
          views::DialogContentType::kControl)));

  auto description_label = std::make_unique<views::Label>(
      controller_->GetDescription(), views::style::CONTEXT_LABEL,
      views::style::STYLE_SECONDARY);
  description_label->SetMultiLine(true);
  description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(std::move(description_label));
  SetExtraView(CreateInfoIcon());
}

BEGIN_METADATA(CredentialLeakDialogView)
END_METADATA

CredentialLeakPrompt* CreateCredentialLeakPromptView(
    CredentialLeakDialogController* controller,
    content::WebContents* web_contents) {
  return new CredentialLeakDialogView(controller, web_contents);
}
