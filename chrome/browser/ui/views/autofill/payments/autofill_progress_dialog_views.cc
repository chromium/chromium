// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/autofill_progress_dialog_views.h"

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/autofill/payments/payments_ui_constants.h"
#include "chrome/browser/ui/autofill/payments/payments_view_factory.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/payments/autofill_error_dialog_view.h"
#include "components/autofill/core/browser/ui/payments/autofill_progress_dialog_controller.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace autofill {

namespace {

// This class owns the Widget which owns the view AutofillProgressDialogViews.
class AutofillProgressDialogViewImpl
    : public AutofillProgressDialogViewDesktop {
 public:
  AutofillProgressDialogViewImpl(
      base::WeakPtr<AutofillProgressDialogController> controller,
      content::WebContents* web_contents);
  AutofillProgressDialogViewImpl(const AutofillProgressDialogViewImpl&) =
      delete;
  AutofillProgressDialogViewImpl& operator=(
      const AutofillProgressDialogViewImpl&) = delete;
  ~AutofillProgressDialogViewImpl() override = default;

  // AutofillProgressDialogViewDesktop:
  views::Widget* GetWidgetForTesting() override;
  void CancelDialogForTesting() override;
  void Dismiss(bool show_confirmation_before_closing,
               bool is_canceled_by_user) override;
  void InvalidateControllerForCallbacks() override;
  base::WeakPtr<AutofillProgressDialogView> GetWeakPtr() override;

 private:
  void CloseWidget(views::Widget::ClosedReason closed_reason);

  std::unique_ptr<views::Widget> dialog_;
};

AutofillProgressDialogViewImpl::AutofillProgressDialogViewImpl(
    base::WeakPtr<AutofillProgressDialogController> controller,
    content::WebContents* web_contents) {
  auto autofill_progress_dialog_view =
      std::make_unique<AutofillProgressDialogViews>(controller);
  auto* tab_interface = tabs::TabInterface::GetFromContents(web_contents);
  CHECK(tab_interface);
  dialog_ = tab_interface->GetTabFeatures()
                ->tab_dialog_manager()
                ->CreateAndShowDialog(
                    autofill_progress_dialog_view.release(),
                    std::make_unique<tabs::TabDialogManager::Params>());
  dialog_->MakeCloseSynchronous(base::BindOnce(
      &AutofillProgressDialogViewImpl::CloseWidget, base::Unretained(this)));
}

void AutofillProgressDialogViewImpl::Dismiss(
    bool show_confirmation_before_closing,
    bool is_canceled_by_user) {
  auto* autofill_progress_dialog_view =
      AsViewClass<AutofillProgressDialogViews>(
          dialog_->GetClientContentsView());
  CHECK(autofill_progress_dialog_view);
  autofill_progress_dialog_view->Dismiss(show_confirmation_before_closing,
                                         is_canceled_by_user);
}

void AutofillProgressDialogViewImpl::InvalidateControllerForCallbacks() {}

base::WeakPtr<AutofillProgressDialogView>
AutofillProgressDialogViewImpl::GetWeakPtr() {
  return nullptr;
}

views::Widget* AutofillProgressDialogViewImpl::GetWidgetForTesting() {
  return dialog_.get();
}

void AutofillProgressDialogViewImpl::CancelDialogForTesting() {
  AsViewClass<AutofillProgressDialogViews>(dialog_->GetClientContentsView())
      ->CancelDialog();
}

void AutofillProgressDialogViewImpl::CloseWidget(
    views::Widget::ClosedReason closed_reason) {
  auto* autofill_progress_dialog_view =
      AsViewClass<AutofillProgressDialogViews>(
          dialog_->GetClientContentsView());
  CHECK(autofill_progress_dialog_view);
  // The following call will result in the destruction of this and, indirectly,
  // the dialog_. Do not access this after the following call.
  autofill_progress_dialog_view->controller()->OnDismissed(
      autofill_progress_dialog_view->is_canceled_by_user() ||
      closed_reason == views::Widget::ClosedReason::kUnspecified ||
      closed_reason == views::Widget::ClosedReason::kCancelButtonClicked);
}

}  // namespace

AutofillProgressDialogViews::AutofillProgressDialogViews(
    base::WeakPtr<AutofillProgressDialogController> controller)
    : controller_(controller) {
  // Set the ownership of the delegate, not the View. The View is owned by the
  // Widget as a child view.
  // TODO(crbug.com/338254375): Remove the following line once this is the
  // default state for widgets.
  SetOwnershipOfNewWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 controller_->GetCancelButtonLabel());
  SetCancelCallback(base::BindOnce(
      &AutofillProgressDialogViews::OnDialogCanceled, base::Unretained(this)));
  SetModalType(ui::mojom::ModalType::kChild);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  SetShowCloseButton(false);
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kControl));

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);

  progress_throbber_ = AddChildView(std::make_unique<views::Throbber>());

  label_ = AddChildView(std::make_unique<views::Label>(
      controller_->GetLoadingMessage(), views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY));
  label_->SetMultiLine(true);
  label_->SetEnabledColor(ui::kColorThrobber);
}

AutofillProgressDialogViews::~AutofillProgressDialogViews() = default;

void AutofillProgressDialogViews::Dismiss(bool show_confirmation_before_closing,
                                          bool is_canceled_by_user) {
  is_canceled_by_user_ = is_canceled_by_user;

  // If `show_confirmation_before_closing` is true, show the confirmation and
  // close the widget with a delay. `show_confirmation_before_closing` being
  // true implies that the user did not cancel the dialog, as it is only set to
  // true once this step in the current flow is completed without any user
  // interaction.
  if (show_confirmation_before_closing && controller_) {
    progress_throbber_->Stop();
    label_->SetText(controller_->GetConfirmationMessage());
    // For accessibility consideration, announce the confirmation message when
    // unmasking is finished.
    label_->GetViewAccessibility().AnnouncePolitely(
        controller_->GetConfirmationMessage());
    progress_throbber_->SetChecked(true);
    GetBubbleFrameView()->SetTitleView(
        std::make_unique<TitleWithIconAfterLabelView>(
            controller_->GetConfirmationTitle(),
            TitleWithIconAfterLabelView::Icon::GOOGLE_PAY));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AutofillProgressDialogViews::CloseWidget,
                       weak_ptr_factory_.GetWeakPtr()),
        kDelayBeforeDismissingProgressDialog);
    return;
  }

  // Otherwise close the widget directly.
  CloseWidget();
}

void AutofillProgressDialogViews::InvalidateControllerForCallbacks() {
  controller_ = nullptr;
}

void AutofillProgressDialogViews::AddedToWidget() {
  DCHECK(progress_throbber_);
  progress_throbber_->Start();

  GetBubbleFrameView()->SetTitleView(
      std::make_unique<TitleWithIconAfterLabelView>(
          GetWindowTitle(), TitleWithIconAfterLabelView::Icon::GOOGLE_PAY));
}

std::u16string AutofillProgressDialogViews::GetWindowTitle() const {
  return controller_ ? controller_->GetLoadingTitle() : u"";
}

void AutofillProgressDialogViews::CloseWidget() {
  GetWidget()->CloseWithReason(
      is_canceled_by_user_ ? views::Widget::ClosedReason::kCancelButtonClicked
                           : views::Widget::ClosedReason::kAcceptButtonClicked);
}

void AutofillProgressDialogViews::OnDialogCanceled() {
  is_canceled_by_user_ = true;
}

std::unique_ptr<AutofillProgressDialogView> CreateAndShowProgressDialog(
    base::WeakPtr<AutofillProgressDialogController> controller,
    content::WebContents* web_contents) {
  return std::make_unique<AutofillProgressDialogViewImpl>(controller,
                                                          web_contents);
}

BEGIN_METADATA(AutofillProgressDialogViews)
END_METADATA

}  // namespace autofill
