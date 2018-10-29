// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_request_dialog_view.h"

#include "base/logging.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/sheet_view_factory.h"
#include "chrome/browser/ui/webauthn/authenticator_request_sheet_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/vector_icons.h"
#include "ui/views/window/dialog_client_view.h"

namespace {

// The material design themed text button with a drop arrow displayed on the
// right side.
class MdTextButtonWithDropArrow : public views::MdTextButton {
 public:
  MdTextButtonWithDropArrow(views::ButtonListener* listener,
                            const base::string16& text)
      : views::MdTextButton(listener, views::style::CONTEXT_BUTTON_MD) {
    SetText(text);
    SetFocusForPlatform();
    SetHorizontalAlignment(gfx::ALIGN_RIGHT);
    SetImageLabelSpacing(views::LayoutProvider::Get()->GetDistanceMetric(
        DISTANCE_RELATED_LABEL_HORIZONTAL_LIST));
    SetDropArrowImage();

    // Reduce padding between the drop arrow and the right border.
    constexpr int kPaddingBetweenBorderAndDropArrow = 8;
    const gfx::Insets original_padding = border()->GetInsets();
    SetBorder(views::CreateEmptyBorder(
        original_padding.top(), original_padding.left(),
        original_padding.bottom(), kPaddingBetweenBorderAndDropArrow));
  }

  ~MdTextButtonWithDropArrow() override {}

 protected:
  void SetDropArrowImage() {
    gfx::ImageSkia drop_arrow_image = gfx::CreateVectorIcon(
        views::kMenuDropArrowIcon,
        color_utils::DeriveDefaultIconColor(label()->enabled_color()));
    SetImage(views::Button::STATE_NORMAL, drop_arrow_image);
  }

  // views::MdTextButton:
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override {
    views::MdTextButton::OnNativeThemeChanged(theme);

    // The icon's color is derived from the label's |enabled_color|, which might
    // have changed as the result of the theme change.
    SetDropArrowImage();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MdTextButtonWithDropArrow);
};

}  // namespace

// static
void ShowAuthenticatorRequestDialog(
    content::WebContents* web_contents,
    std::unique_ptr<AuthenticatorRequestDialogModel> model) {
  // The authenticator request dialog will only be shown for common user-facing
  // WebContents, which have a |manager|. Most other sources without managers,
  // like service workers and extension background pages, do not allow WebAuthn
  // requests to be issued in the first place.
  // TODO(https://crbug.com/849323): There are some niche WebContents where the
  // WebAuthn API is available, but there is no |manager| available. Currently,
  // we will not be able to show a dialog, so the |model| will be immediately
  // destroyed. The request may be able to still run to completion if it does
  // not require any user input, otherise it will be blocked and time out. We
  // should audit this.
  auto* manager = web_modal::WebContentsModalDialogManager::FromWebContents(
      constrained_window::GetTopLevelWebContents(web_contents));
  if (!manager)
    return;

  // Keep this logic in sync with AuthenticatorRequestDialogViewTestApi::Show.
  auto dialog = std::make_unique<AuthenticatorRequestDialogView>(
      web_contents, std::move(model));
  constrained_window::ShowWebModalDialogViews(dialog.release(), web_contents);
}

AuthenticatorRequestDialogView::AuthenticatorRequestDialogView(
    content::WebContents* web_contents,
    std::unique_ptr<AuthenticatorRequestDialogModel> model)
    : content::WebContentsObserver(web_contents),
      model_(std::move(model)),
      sheet_(nullptr) {
  model_->AddObserver(this);

  // Currently, all sheets have a label on top and controls at the bottom.
  // Consider moving this to AuthenticatorRequestSheetView if this changes.
  SetLayoutManager(std::make_unique<views::FillLayout>());
  OnStepTransition();
}

AuthenticatorRequestDialogView::~AuthenticatorRequestDialogView() {
  model_->RemoveObserver(this);

  // AuthenticatorRequestDialogView is a WidgetDelegate, owned by views::Widget.
  // It's only destroyed by Widget::OnNativeWidgetDestroyed() invoking
  // DeleteDelegate(), and because WIDGET_OWNS_NATIVE_WIDGET, ~Widget() is
  // invoked straight after, which destroys child views. views::View subclasses
  // shouldn't be doing anything interesting in their destructors, so it should
  // be okay to destroy the |sheet_| immediately after this line.
  //
  // However, as AuthenticatorRequestDialogModel is owned by |this|, and
  // ObservableAuthenticatorList is owned by AuthenticatorRequestDialogModel,
  // destroy all view components that might own models observing the list prior
  // to destroying AuthenticatorRequestDialogModel.
  RemoveAllChildViews(true /* delete_children */);
}

gfx::Size AuthenticatorRequestDialogView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);
  return gfx::Size(width, GetHeightForWidth(width));
}

views::View* AuthenticatorRequestDialogView::CreateExtraView() {
  other_transports_button_ = new MdTextButtonWithDropArrow(
      this, l10n_util::GetStringUTF16(IDS_WEBAUTHN_TRANSPORT_POPUP_LABEL));
  ToggleOtherTransportsButtonVisibility();
  return other_transports_button_;
}

bool AuthenticatorRequestDialogView::Accept() {
  sheet()->model()->OnAccept();
  return false;
}

bool AuthenticatorRequestDialogView::Cancel() {
  sheet()->model()->OnCancel();
  return false;
}

bool AuthenticatorRequestDialogView::Close() {
  // To keep the UI responsive, always allow immediately closing the dialog when
  // desired; but still trigger cancelling the AuthenticatorRequest unless it is
  // already complete.
  //
  // Note that on most sheets, cancelling will immediately destroy the request,
  // so this method will be re-entered like so:
  //
  //   AuthenticatorRequestDialogView::Close()
  //   views::DialogClientView::CanClose()
  //   views::Widget::Close()
  //   AuthenticatorRequestDialogView::OnStepTransition()
  //   AuthenticatorRequestDialogModel::SetCurrentStep()
  //   AuthenticatorRequestDialogModel::OnRequestComplete()
  //   ChromeAuthenticatorRequestDelegate::~ChromeAuthenticatorRequestDelegate()
  //   content::AuthenticatorImpl::InvokeCallbackAndCleanup()
  //   content::AuthenticatorImpl::FailWithNotAllowedErrorAndCleanup()
  //   <<invoke callback>>
  //   ChromeAuthenticatorRequestDelegate::OnCancelRequest()
  //   AuthenticatorRequestDialogModel::Cancel()
  //   AuthenticatorRequestDialogView::Cancel()
  //   AuthenticatorRequestDialogView::Close()  [initial call]
  //
  // This should not be a problem as the native widget will never synchronously
  // close and hence not synchronously destroy the model while it's iterating
  // over observers in SetCurrentStep().
  if (!model_->is_request_complete())
    Cancel();

  return true;
}

int AuthenticatorRequestDialogView::GetDialogButtons() const {
  int button_mask = 0;
  if (sheet()->model()->IsAcceptButtonVisible())
    button_mask |= ui::DIALOG_BUTTON_OK;
  if (sheet()->model()->IsCancelButtonVisible())
    button_mask |= ui::DIALOG_BUTTON_CANCEL;
  return button_mask;
}

int AuthenticatorRequestDialogView::GetDefaultDialogButton() const {
  // The default button is either the `Ok` button or nothing.
  if (sheet()->model()->IsAcceptButtonVisible())
    return ui::DIALOG_BUTTON_OK;
  return ui::DIALOG_BUTTON_NONE;
}

base::string16 AuthenticatorRequestDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  switch (button) {
    case ui::DIALOG_BUTTON_NONE:
      break;
    case ui::DIALOG_BUTTON_OK:
      return sheet()->model()->GetAcceptButtonLabel();
    case ui::DIALOG_BUTTON_CANCEL:
      return sheet()->model()->GetCancelButtonLabel();
  }
  NOTREACHED();
  return base::string16();
}

bool AuthenticatorRequestDialogView::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  switch (button) {
    case ui::DIALOG_BUTTON_NONE:
      break;
    case ui::DIALOG_BUTTON_OK:
      return sheet()->model()->IsAcceptButtonEnabled();
    case ui::DIALOG_BUTTON_CANCEL:
      return true;  // Cancel is always enabled if visible.
  }
  NOTREACHED();
  return false;
}

views::View* AuthenticatorRequestDialogView::GetInitiallyFocusedView() {
  // Need to provide a custom implementation, as most dialog sheets will not
  // have a default button which gets initial focus. The focus priority is:
  //  1. Step-specific content, e.g. transport selection list, if any.
  //  2. Accept button, if visible and enabled.
  //  3. Other transport selection button, if visible.
  //  4. `Cancel` / `Close` button.

  views::View* intially_focused_sheet_control =
      sheet()->GetInitiallyFocusedView();
  if (intially_focused_sheet_control)
    return intially_focused_sheet_control;

  if (sheet()->model()->IsAcceptButtonVisible() &&
      sheet()->model()->IsAcceptButtonEnabled()) {
    return GetDialogClientView()->ok_button();
  }

  if (ShouldOtherTransportsButtonBeVisible())
    return other_transports_button_;

  if (sheet()->model()->IsCancelButtonVisible())
    return GetDialogClientView()->cancel_button();

  return nullptr;
}

ui::ModalType AuthenticatorRequestDialogView::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

base::string16 AuthenticatorRequestDialogView::GetWindowTitle() const {
  return sheet()->model()->GetStepTitle();
}

bool AuthenticatorRequestDialogView::ShouldShowWindowTitle() const {
  return false;
}

bool AuthenticatorRequestDialogView::ShouldShowCloseButton() const {
  return false;
}

void AuthenticatorRequestDialogView::OnModelDestroyed() {
  NOTREACHED();
}

void AuthenticatorRequestDialogView::OnStepTransition() {
  ReplaceCurrentSheetWith(CreateSheetViewForCurrentStepOf(model_.get()));

  if (model_->should_dialog_be_closed()) {
    if (!GetWidget())
      return;
    GetWidget()->Close();
  }
}

void AuthenticatorRequestDialogView::OnSheetModelChanged() {
  UpdateUIForCurrentSheet();
}

void AuthenticatorRequestDialogView::ButtonPressed(views::Button* sender,
                                                   const ui::Event& event) {
  DCHECK_EQ(sender, other_transports_button_);

  auto* other_transports_menu_model =
      sheet_->model()->GetOtherTransportsMenuModel();
  DCHECK(other_transports_menu_model);
  DCHECK_GE(other_transports_menu_model->GetItemCount(), 1);

  other_transports_menu_runner_ = std::make_unique<views::MenuRunner>(
      other_transports_menu_model, views::MenuRunner::COMBOBOX);

  gfx::Rect anchor_bounds = other_transports_button_->GetBoundsInScreen();
  other_transports_menu_runner_->RunMenuAt(
      other_transports_button_->GetWidget(), nullptr /* menu_button */,
      anchor_bounds, views::MENU_ANCHOR_TOPLEFT, ui::MENU_SOURCE_MOUSE);
}

void AuthenticatorRequestDialogView::ReplaceCurrentSheetWith(
    std::unique_ptr<AuthenticatorRequestSheetView> new_sheet) {
  DCHECK(new_sheet);

  other_transports_menu_runner_.reset();

  delete sheet_;
  DCHECK(!has_children());

  sheet_ = new_sheet.get();
  AddChildView(new_sheet.release());

  UpdateUIForCurrentSheet();
}

void AuthenticatorRequestDialogView::UpdateUIForCurrentSheet() {
  DCHECK(sheet_);

  sheet_->ReInitChildViews();

  // Whether to show the `Choose another option` button, or other dialog
  // configuration is delegated to the |sheet_|, and the new sheet likely wants
  // to provide a new configuration.
  ToggleOtherTransportsButtonVisibility();
  DialogModelChanged();

  // If the widget is not yet shown or already being torn down, we are done. In
  // the former case, sizing/layout will happen once the dialog is visible.
  if (!GetWidget())
    return;

  // Force re-layout of the entire dialog client view, which includes the sheet
  // content as well as the button row on the bottom.
  DCHECK(GetDialogClientView());
  GetDialogClientView()->Layout();

  // The accessibility title is also sourced from the |sheet_|'s step title.
  GetWidget()->UpdateWindowTitle();

  // TODO(https://crbug.com/849323): Investigate how a web-modal dialog's
  // lifetime compares to that of the parent WebContents. Take a conservative
  // approach for now.
  if (!web_contents())
    return;

  // The |dialog_manager| might temporarily be unavailable while te tab is being
  // dragged from one browser window to the other.
  auto* dialog_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents());
  if (!dialog_manager)
    return;

  // Update the dialog size and position, as the preferred size of the sheet
  // might have changed.
  constrained_window::UpdateWebContentsModalDialogPosition(
      GetWidget(),
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents())
          ->delegate()
          ->GetWebContentsModalDialogHost());

  // Reset focus to the highest priority control on the new/updated sheet.
  if (GetInitiallyFocusedView())
    GetInitiallyFocusedView()->RequestFocus();
}

void AuthenticatorRequestDialogView::ToggleOtherTransportsButtonVisibility() {
  // The button is not yet created when this is called for the first time.
  if (!other_transports_button_)
    return;

  other_transports_button_->SetVisible(ShouldOtherTransportsButtonBeVisible());
}

bool AuthenticatorRequestDialogView::ShouldOtherTransportsButtonBeVisible()
    const {
  return sheet_->model()->GetOtherTransportsMenuModel() &&
         sheet_->model()->GetOtherTransportsMenuModel()->GetItemCount();
}
