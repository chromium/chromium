// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/credential_leak_dialog_view.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/ui/passwords/credential_leak_dialog_controller.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/theme_resources.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/tabs/public/tab_interface.h"
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
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

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

class CredentialLeakPromptImpl : public CredentialLeakPrompt {
 public:
  CredentialLeakPromptImpl(CredentialLeakDialogController* controller,
                           content::WebContents* web_contents);
  CredentialLeakPromptImpl(const CredentialLeakPromptImpl&) = delete;
  CredentialLeakPromptImpl& operator=(const CredentialLeakPromptImpl&) = delete;
  ~CredentialLeakPromptImpl() override = default;

  // Overrides from CredentialLeakPrompt:
  void ShowCredentialLeakPrompt() override;
  views::Widget* GetWidgetForTesting() override;

 private:
  // Callback to make Widget::Close synchronous.
  void CloseWidget(views::Widget::ClosedReason closed_reason);

  std::unique_ptr<CredentialLeakDialogView> credential_leak_dialog_view_;
  std::unique_ptr<views::Widget> dialog_;
};

CredentialLeakPromptImpl::CredentialLeakPromptImpl(
    CredentialLeakDialogController* controller,
    content::WebContents* web_contents) {
  credential_leak_dialog_view_ =
      std::make_unique<CredentialLeakDialogView>(controller, web_contents);
}

void CredentialLeakPromptImpl::ShowCredentialLeakPrompt() {
  CHECK(credential_leak_dialog_view_);
  auto* tab_interface = tabs::TabInterface::GetFromContents(
      credential_leak_dialog_view_->web_contents());
  CHECK(tab_interface);
  if (tab_interface->CanShowModalUI()) {
    credential_leak_dialog_view_->InitWindow();
    dialog_ = tab_interface->GetTabFeatures()
                  ->tab_dialog_manager()
                  ->CreateAndShowDialog(
                      credential_leak_dialog_view_.release(),
                      std::make_unique<tabs::TabDialogManager::Params>());
    dialog_->MakeCloseSynchronous(base::BindOnce(
        &CredentialLeakPromptImpl::CloseWidget, base::Unretained(this)));
  }
}

views::Widget* CredentialLeakPromptImpl::GetWidgetForTesting() {
  return dialog_.get();
}

void CredentialLeakPromptImpl::CloseWidget(
    views::Widget::ClosedReason closed_reason) {
  auto* credential_leak_dialog_view =
      AsViewClass<CredentialLeakDialogView>(dialog_->GetClientContentsView());
  CHECK(credential_leak_dialog_view);
  // Tell the controller to destroy its reference this class which will also
  // destroy the |dialog_|.
  credential_leak_dialog_view->controller()->ResetDialog();
}

}  // namespace

CredentialLeakDialogView::CredentialLeakDialogView(
    CredentialLeakDialogController* controller,
    content::WebContents* web_contents)
    : controller_(controller), web_contents_(web_contents) {
  CHECK(controller);
  CHECK(web_contents);

  // Set the ownership of the delegate, not the View. The View is owned by the
  // Widget as a child view.
  // TODO(crbug.com/338254375): Remove the following line once this is the
  // default state for widgets.
  SetOwnershipOfNewWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

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
    // further calls to the controller and inhibit recursive closes, and invoke
    // the provided method on the controller.
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

CredentialLeakDialogView::~CredentialLeakDialogView() = default;

void CredentialLeakDialogView::AddedToWidget() {
  // Set the header image.
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  auto image_view = std::make_unique<ThemeTrackingNonAccessibleImageView>(
      *bundle.GetImageSkiaNamed(IDR_PASSWORD_CHECK),
      *bundle.GetImageSkiaNamed(IDR_PASSWORD_CHECK_DARK),
      base::BindRepeating(&views::BubbleFrameView::background_color,
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

std::unique_ptr<CredentialLeakPrompt> CreateCredentialLeakPromptView(
    CredentialLeakDialogController* controller,
    content::WebContents* web_contents) {
  return std::make_unique<CredentialLeakPromptImpl>(controller, web_contents);
}
