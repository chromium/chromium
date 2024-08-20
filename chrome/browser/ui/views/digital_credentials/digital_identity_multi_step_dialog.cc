// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/digital_credentials/digital_identity_multi_step_dialog.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-shared.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_utils.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"

namespace {

using ButtonModel = ui::DialogModel::Button;

// Creates ScrollView for `contents_view`.
std::unique_ptr<views::View> CreateContentsScrollView(
    std::unique_ptr<views::View> contents_view) {
  int kMaxDialogHeight = views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_SCROLLABLE_AREA_MAX_HEIGHT);
  auto scroll_view = std::make_unique<views::ScrollView>();
  scroll_view->ClipHeightTo(0, kMaxDialogHeight);
  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  scroll_view->SetContents(std::move(contents_view));
  return std::move(scroll_view);
}

ButtonModel CreateButtonModel(const ButtonModel::Params& params) {
  return ButtonModel(base::DoNothingAs<void(const ui::Event&)>(), params);
}

}  // anonymous namespace

DigitalIdentityMultiStepDialog::TestApi::TestApi(
    DigitalIdentityMultiStepDialog* dialog)
    : dialog_(dialog) {}
DigitalIdentityMultiStepDialog::TestApi::~TestApi() = default;

DigitalIdentityMultiStepDialog::Delegate::Delegate()
    : views::BubbleDialogDelegate(/*anchor_view=*/nullptr,
                                  views::BubbleBorder::Arrow::NONE,
                                  views::BubbleBorder::DIALOG_SHADOW,
                                  /*autosize=*/true) {
  SetModalType(ui::mojom::ModalType::kChild);
  SetShowCloseButton(true);

  auto contents_view_unique = std::make_unique<views::BoxLayoutView>();
  contents_view_unique->SetOrientation(views::LayoutOrientation::kVertical);

  contents_view_ = contents_view_unique.get();
  SetContentsView(CreateContentsScrollView(std::move(contents_view_unique)));

  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
}

DigitalIdentityMultiStepDialog::Delegate::~Delegate() = default;

void DigitalIdentityMultiStepDialog::Delegate::Update(
    const std::optional<ButtonModel::Params>& accept_button,
    base::OnceClosure accept_callback,
    const ButtonModel::Params& cancel_button,
    base::OnceClosure cancel_callback,
    const std::u16string& dialog_title,
    const std::u16string& body_text,
    std::unique_ptr<views::View> custom_body_field) {
  accept_callback_ = std::move(accept_callback);
  cancel_callback_ = std::move(cancel_callback);

  if (accept_button) {
    SetAcceptCallbackWithClose(base::BindRepeating(&Delegate::OnDialogAccepted,
                                                   base::Unretained(this)));
  }

  SetTitle(dialog_title);
  SetCancelCallbackWithClose(
      base::BindRepeating(&Delegate::OnDialogCanceled, base::Unretained(this)));
  SetCloseCallback(base::BindOnce(&Delegate::OnDialogClosed,
                                  weak_ptr_factory_.GetWeakPtr()));

  int button_mask = static_cast<int>(ui::mojom::DialogButton::kCancel);
  if (accept_button) {
    button_mask |= static_cast<int>(ui::mojom::DialogButton::kOk);
    views::ConfigureBubbleButtonForParams(*this, /*button_view=*/nullptr,
                                          ui::mojom::DialogButton::kOk,
                                          CreateButtonModel(*accept_button));
  }
  views::ConfigureBubbleButtonForParams(*this, /*button_view=*/nullptr,
                                        ui::mojom::DialogButton::kCancel,
                                        CreateButtonModel(cancel_button));

  SetButtons(button_mask);

  auto body_label = std::make_unique<views::Label>(body_text);
  body_label->SetMultiLine(true);
  body_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  contents_view_->RemoveAllChildViews();
  contents_view_->AddChildView(std::move(body_label));
  if (custom_body_field) {
    contents_view_->AddChildView(std::move(custom_body_field));
  }
}

bool DigitalIdentityMultiStepDialog::Delegate::OnDialogAccepted() {
  closed_reason_ = views::Widget::ClosedReason::kAcceptButtonClicked;

  // views::DialogDelegate does not support synchronously destroying
  // views::Widget from accept callback.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(accept_callback_));

  // Reset callback in case user clicks button again prior to posted task being
  // run.
  ResetCallbacks();

  // Destroying `this` will close the dialog.
  return false;
}

bool DigitalIdentityMultiStepDialog::Delegate::OnDialogCanceled() {
  closed_reason_ = views::Widget::ClosedReason::kCancelButtonClicked;

  // views::DialogDelegate does not support synchronously destroying
  // views::Widget from cancel callback.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(cancel_callback_));

  // Reset callback in case user clicks button again prior to posted task being
  // run.
  ResetCallbacks();

  // Destroying `this` will close the dialog.
  return false;
}

void DigitalIdentityMultiStepDialog::Delegate::OnDialogClosed() {
  if (cancel_callback_) {
    // views::DialogDelegate does not support synchronously destroying
    // views::Widget from close callback.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(cancel_callback_));
  }
}

void DigitalIdentityMultiStepDialog::Delegate::ResetCallbacks() {
  SetAcceptCallbackWithClose(base::BindRepeating([]() { return false; }));
  SetCancelCallbackWithClose(base::BindRepeating([]() { return false; }));
  SetCloseCallback(base::OnceClosure());
}

DigitalIdentityMultiStepDialog::DigitalIdentityMultiStepDialog(
    base::WeakPtr<content::WebContents> web_contents)
    : web_contents_(web_contents) {}

DigitalIdentityMultiStepDialog::~DigitalIdentityMultiStepDialog() {
  if (dialog_ && !dialog_->IsClosed()) {
    dialog_->CloseWithReason(GetWidgetDelegate()->get_closed_reason());
  }
}

void DigitalIdentityMultiStepDialog::TryShow(
    const std::optional<ui::DialogModel::Button::Params>& accept_button,
    base::OnceClosure accept_callback,
    const ui::DialogModel::Button::Params& cancel_button,
    base::OnceClosure cancel_callback,
    const std::u16string& dialog_title,
    const std::u16string& body_text,
    std::unique_ptr<views::View> custom_body_field) {
  if (!web_contents_) {
    // Post task so that the callback is guaranteed to be called asynchronously
    // in all cases.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(cancel_callback));
    return;
  }

  std::unique_ptr<Delegate> new_dialog_delegate;
  Delegate* delegate = GetWidgetDelegate();
  if (!delegate) {
    new_dialog_delegate = std::make_unique<Delegate>();
    delegate = new_dialog_delegate.get();
  }

  CHECK(delegate);
  delegate->Update(accept_button, std::move(accept_callback), cancel_button,
                   std::move(cancel_callback), dialog_title, body_text,
                   std::move(custom_body_field));

  if (new_dialog_delegate) {
    // views::Widget takes ownership of `new_dialog_delegate`.
    dialog_ = constrained_window::ShowWebModalDialogViews(
                  new_dialog_delegate.release(), web_contents_.get())
                  ->GetWeakPtr();
  }
}

DigitalIdentityMultiStepDialog::Delegate*
DigitalIdentityMultiStepDialog::GetWidgetDelegate() {
  if (!dialog_) {
    return nullptr;
  }
  return reinterpret_cast<Delegate*>(dialog_->widget_delegate());
}
