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
#include "ui/color/color_variant.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_utils.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"

namespace {

using ButtonModel = ui::DialogModel::Button;

// A top margin is necessary since without the dialog "close" button, there is
// no enough margin to display the progress bar and match the margin below the
// content view.
constexpr int kContentMarginTop = 12;

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

// The wrapped views::BubbleDialogDelegate.
class DigitalIdentityMultiStepDialogDelegate
    : public views::BubbleDialogDelegate {
 public:
  DigitalIdentityMultiStepDialogDelegate();
  ~DigitalIdentityMultiStepDialogDelegate() override;

  void Update(
      const std::optional<ui::DialogModel::Button::Params>& accept_button,
      base::OnceClosure accept_callback,
      const ui::DialogModel::Button::Params& cancel_button,
      base::OnceClosure cancel_callback,
      const std::u16string& dialog_title,
      const std::u16string& body_text,
      std::unique_ptr<views::View> custom_body_field);

  views::Widget::ClosedReason get_closed_reason() { return closed_reason_; }

 private:
  bool OnDialogAccepted();
  bool OnDialogCanceled();
  void OnDialogClosed();

  void ResetCallbacks();

  // Owned by the parent view.
  raw_ptr<views::View> contents_view_;

  base::OnceClosure accept_callback_;
  base::OnceClosure cancel_callback_;

  views::Widget::ClosedReason closed_reason_ =
      views::Widget::ClosedReason::kUnspecified;

  base::WeakPtrFactory<DigitalIdentityMultiStepDialogDelegate>
      weak_ptr_factory_{this};
};

DigitalIdentityMultiStepDialogDelegate::DigitalIdentityMultiStepDialogDelegate()
    : views::BubbleDialogDelegate(/*anchor_view=*/nullptr,
                                  views::BubbleBorder::Arrow::NONE,
                                  views::BubbleBorder::DIALOG_SHADOW,
                                  /*autosize=*/true) {
  SetOwnedByWidget(OwnedByWidgetPassKey());
  SetModalType(ui::mojom::ModalType::kChild);
  SetShowCloseButton(true);

  auto contents_view_unique = std::make_unique<views::BoxLayoutView>();
  contents_view_unique->SetOrientation(views::LayoutOrientation::kVertical);

  contents_view_ = contents_view_unique.get();
  SetContentsView(CreateContentsScrollView(std::move(contents_view_unique)));

  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
}

DigitalIdentityMultiStepDialogDelegate::
    ~DigitalIdentityMultiStepDialogDelegate() = default;

void DigitalIdentityMultiStepDialogDelegate::Update(
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
    SetAcceptCallbackWithClose(base::BindRepeating(
        &DigitalIdentityMultiStepDialogDelegate::OnDialogAccepted,
        base::Unretained(this)));
  }

  SetTitle(dialog_title);
  // When the `dialog_title` is empty, the calling site would make sure to add
  // the proper name for the `custom_body_field` accessibility. This should be
  // used in this case as the dialog accessible title.
  SetAccessibleTitle(
      dialog_title.empty()
          ? custom_body_field->GetViewAccessibility().GetCachedName()
          : dialog_title);

  SetShowCloseButton(false);
  SetCancelCallbackWithClose(base::BindRepeating(
      &DigitalIdentityMultiStepDialogDelegate::OnDialogCanceled,
      base::Unretained(this)));
  SetCloseCallback(
      base::BindOnce(&DigitalIdentityMultiStepDialogDelegate::OnDialogClosed,
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

  contents_view_->RemoveAllChildViews();
  if (!body_text.empty()) {
    auto body_label = std::make_unique<views::Label>(body_text);
    body_label->SetMultiLine(true);
    body_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    contents_view_->AddChildView(std::move(body_label));
  }
  if (custom_body_field) {
    contents_view_->AddChildView(std::move(custom_body_field));
  }
}

bool DigitalIdentityMultiStepDialogDelegate::OnDialogAccepted() {
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

bool DigitalIdentityMultiStepDialogDelegate::OnDialogCanceled() {
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

void DigitalIdentityMultiStepDialogDelegate::OnDialogClosed() {
  if (cancel_callback_) {
    // views::DialogDelegate does not support synchronously destroying
    // views::Widget from close callback.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(cancel_callback_));
  }
}

void DigitalIdentityMultiStepDialogDelegate::ResetCallbacks() {
  SetAcceptCallbackWithClose(base::BindRepeating([]() { return false; }));
  SetCancelCallbackWithClose(base::BindRepeating([]() { return false; }));
  SetCloseCallback(base::OnceClosure());
}

DigitalIdentityMultiStepDialog::TestApi::TestApi(
    DigitalIdentityMultiStepDialog* dialog)
    : dialog_(dialog) {}

DigitalIdentityMultiStepDialog::TestApi::~TestApi() = default;

views::Widget* DigitalIdentityMultiStepDialog::TestApi::GetWidget() {
  return dialog_->dialog_.get();
}

views::BubbleDialogDelegate*
DigitalIdentityMultiStepDialog::TestApi::GetWidgetDelegate() {
  return dialog_->GetWidgetDelegate();
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
    std::unique_ptr<views::View> custom_body_field,
    bool show_progress_bar) {
  if (!web_contents_) {
    // Post task so that the callback is guaranteed to be called asynchronously
    // in all cases.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(cancel_callback));
    return;
  }
  views::View* custom_body_field_ptr = custom_body_field.get();

  std::unique_ptr<DigitalIdentityMultiStepDialogDelegate> new_dialog_delegate;
  DigitalIdentityMultiStepDialogDelegate* delegate = GetWidgetDelegate();
  if (!delegate) {
    new_dialog_delegate =
        std::make_unique<DigitalIdentityMultiStepDialogDelegate>();
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
  if (dialog_title.empty()) {
    // Adding a top margin is necessary only when there is no title in which
    // case the content is very close to the dialog top.
    delegate->GetBubbleFrameView()->SetContentMargins(
        gfx::Insets::TLBR(kContentMarginTop, 0, 0, 0));
  }
  if (!new_dialog_delegate && custom_body_field_ptr) {
    // When the dialog is displayed for the first time, the title is announced
    // to screen reader. However, upon subsequent changes to the dialog contents
    // including the title, this has to be announced explicitly to convey to
    // the user the change in the UI.
    views::ViewAccessibility& custom_body_field_accessibility =
        custom_body_field_ptr->GetViewAccessibility();
    custom_body_field_accessibility.AnnouncePolitely(
        dialog_title.empty() ? custom_body_field_accessibility.GetCachedName()
                             : dialog_title);
  }
  delegate->GetBubbleFrameView()->SetProgress(
      show_progress_bar ? std::optional<double>(-1) : std::nullopt);
}

ui::ColorVariant DigitalIdentityMultiStepDialog::GetBackgroundColor() {
  DigitalIdentityMultiStepDialogDelegate* widget_delegate = GetWidgetDelegate();
  if (!widget_delegate) {
    return gfx::kPlaceholderColor;
  }
  return widget_delegate->background_color();
}

DigitalIdentityMultiStepDialogDelegate*
DigitalIdentityMultiStepDialog::GetWidgetDelegate() {
  if (!dialog_) {
    return nullptr;
  }
  return static_cast<DigitalIdentityMultiStepDialogDelegate*>(
      dialog_->widget_delegate());
}
