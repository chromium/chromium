// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_request_dialog_view.h"

#include <string>

#include "base/logging.h"
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
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

// static
void ShowAuthenticatorRequestDialog(content::WebContents* web_contents,
                                    AuthenticatorRequestDialogModel* model) {
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
  if (!manager) {
    return;
  }

  new AuthenticatorRequestDialogView(web_contents, model);
}

AuthenticatorRequestDialogView::~AuthenticatorRequestDialogView() {
  if (model_) {
    model_->RemoveObserver(this);
  }

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
  RemoveAllChildViews();
}

void AuthenticatorRequestDialogView::ReplaceCurrentSheetWith(
    std::unique_ptr<AuthenticatorRequestSheetView> new_sheet) {
  DCHECK(new_sheet);

  other_mechanisms_menu_runner_.reset();

  delete sheet_;
  DCHECK(children().empty());

  sheet_ = new_sheet.get();
  AddChildView(new_sheet.release());

  UpdateUIForCurrentSheet();
}

void AuthenticatorRequestDialogView::UpdateUIForCurrentSheet() {
  DCHECK(sheet_);

  sheet_->ReInitChildViews();

  int buttons = ui::DIALOG_BUTTON_NONE;
  if (sheet()->model()->IsAcceptButtonVisible()) {
    buttons |= ui::DIALOG_BUTTON_OK;
  }
  if (sheet()->model()->IsCancelButtonVisible()) {
    buttons |= ui::DIALOG_BUTTON_CANCEL;
  }
  SetButtons(buttons);
  SetDefaultButton((buttons & ui::DIALOG_BUTTON_OK) ? ui::DIALOG_BUTTON_OK
                                                    : ui::DIALOG_BUTTON_NONE);
  SetButtonLabel(ui::DIALOG_BUTTON_OK, sheet_->model()->GetAcceptButtonLabel());
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 sheet_->model()->GetCancelButtonLabel());

  // Whether to show the `Choose another option` button, or other dialog
  // configuration is delegated to the |sheet_|, and the new sheet likely wants
  // to provide a new configuration.
  other_mechanisms_button_->SetVisible(ShouldOtherMechanismsButtonBeVisible());
  other_mechanisms_button_->SetText(
      sheet_->model()->GetOtherMechanismButtonLabel());
  manage_devices_button_->SetVisible(
      sheet_->model()->IsManageDevicesButtonVisible());
  DialogModelChanged();

  // If the widget is not yet shown or already being torn down, we are done. In
  // the former case, sizing/layout will happen once the dialog is visible.
  if (!GetWidget()) {
    return;
  }

  // Force re-layout of the entire dialog client view, which includes the sheet
  // content as well as the button row on the bottom.
  // TODO(ellyjones): Why is this necessary?
  GetWidget()->GetRootView()->Layout();

  // The accessibility title is also sourced from the |sheet_|'s step title.
  GetWidget()->UpdateWindowTitle();

  // TODO(https://crbug.com/849323): Investigate how a web-modal dialog's
  // lifetime compares to that of the parent WebContents. Take a conservative
  // approach for now.
  if (!web_contents()) {
    return;
  }

  // The |dialog_manager| might temporarily be unavailable while the tab is
  // being dragged from one browser window to the other.
  auto* dialog_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(
          constrained_window::GetTopLevelWebContents(web_contents()));
  if (!dialog_manager) {
    return;
  }

  // Update the dialog size and position, as the preferred size of the sheet
  // might have changed.
  constrained_window::UpdateWebContentsModalDialogPosition(
      GetWidget(), dialog_manager->delegate()->GetWebContentsModalDialogHost());

  // Reset focus to the highest priority control on the new/updated sheet.
  if (GetInitiallyFocusedView()) {
    GetInitiallyFocusedView()->RequestFocus();
  }
}

bool AuthenticatorRequestDialogView::ShouldOtherMechanismsButtonBeVisible()
    const {
  return sheet_->model()->IsOtherMechanismButtonVisible();
}

bool AuthenticatorRequestDialogView::Accept() {
  sheet()->model()->OnAccept();
  return false;
}

bool AuthenticatorRequestDialogView::Cancel() {
  sheet()->model()->OnCancel();
  return false;
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
  NOTREACHED_NORETURN();
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
  if (intially_focused_sheet_control) {
    return intially_focused_sheet_control;
  }

  if (sheet()->model()->IsAcceptButtonVisible() &&
      sheet()->model()->IsAcceptButtonEnabled()) {
    return GetOkButton();
  }

  if (ShouldOtherMechanismsButtonBeVisible()) {
    return other_mechanisms_button_;
  }

  if (sheet()->model()->IsCancelButtonVisible()) {
    return GetCancelButton();
  }

  return nullptr;
}

std::u16string AuthenticatorRequestDialogView::GetWindowTitle() const {
  return sheet()->model()->GetStepTitle();
}

void AuthenticatorRequestDialogView::OnModelDestroyed(
    AuthenticatorRequestDialogModel* model) {
  model_ = nullptr;
}

void AuthenticatorRequestDialogView::OnStepTransition() {
  DCHECK(model_) << "Model must be valid since this is a model observer method";
  if (model_->should_dialog_be_closed()) {
    if (!first_shown_) {
      // No widget has ever been created for this dialog, thus there will be no
      // DeleteDelegate() call to delete this view.
      DCHECK(!GetWidget());
      delete this;
      return;
    }
    if (GetWidget()) {
      GetWidget()->Close();  // DeleteDelegate() will delete |this|.
    }
    return;
  }
  ReplaceCurrentSheetWith(CreateSheetViewForCurrentStepOf(model_));
  Show();
}

void AuthenticatorRequestDialogView::OnSheetModelChanged() {
  UpdateUIForCurrentSheet();
}

void AuthenticatorRequestDialogView::OnVisibilityChanged(
    content::Visibility visibility) {
  const bool web_contents_was_hidden = web_contents_hidden_;
  web_contents_hidden_ = visibility == content::Visibility::HIDDEN;

  // Show() does not actually show the dialog while the parent WebContents are
  // hidden. Instead, show it when the WebContents become visible again.
  if (web_contents_was_hidden && !web_contents_hidden_ &&
      !GetWidget()->IsVisible()) {
    GetWidget()->Show();
  }
}

AuthenticatorRequestDialogView::AuthenticatorRequestDialogView(
    content::WebContents* web_contents,
    AuthenticatorRequestDialogModel* model)
    : content::WebContentsObserver(web_contents),
      model_(model),
      web_contents_hidden_(web_contents->GetVisibility() ==
                           content::Visibility::HIDDEN) {
  SetShowTitle(false);
  DCHECK(!model_->should_dialog_be_closed());
  model_->AddObserver(this);

  // This View contains buttons that can appear at the bottom left of the
  // dialog. Only a single button is expected to be visible at a time so the
  // padding between them is zero.
  auto hbox = std::make_unique<views::View>();
  hbox->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 0));

  other_mechanisms_button_ = new views::MdTextButton(base::BindRepeating(
      &AuthenticatorRequestDialogView::OtherMechanismsButtonPressed,
      base::Unretained(this)));
  hbox->AddChildView(other_mechanisms_button_.get());

  manage_devices_button_ = new views::MdTextButton(
      base::BindRepeating(
          &AuthenticatorRequestDialogView::ManageDevicesButtonPressed,
          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_WEBAUTHN_MANAGE_DEVICES));
  hbox->AddChildView(manage_devices_button_.get());

  SetExtraView(std::move(hbox));

  SetCloseCallback(
      base::BindOnce(&AuthenticatorRequestDialogView::OnDialogClosing,
                     base::Unretained(this)));

  SetModalType(ui::MODAL_TYPE_CHILD);
  SetShowCloseButton(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  // Currently, all sheets have a label on top and controls at the bottom.
  // Consider moving this to AuthenticatorRequestSheetView if this changes.
  SetLayoutManager(std::make_unique<views::FillLayout>());

  OnStepTransition();
}

void AuthenticatorRequestDialogView::Show() {
  if (!first_shown_) {
    constrained_window::ShowWebModalDialogViews(this, web_contents());
    DCHECK(GetWidget());
    first_shown_ = true;
    return;
  }

  if (web_contents_hidden_) {
    // Calling Widget::Show() while the tab is not in foreground shows the
    // dialog on the foreground tab (https://crbug/969153). Instead, wait for
    // OnVisibilityChanged() to signal the tab going into foreground again, and
    // then show the widget.
    return;
  }

  GetWidget()->Show();
}

void AuthenticatorRequestDialogView::OtherMechanismsButtonPressed() {
  sheet_->model()->OnBack();
}

void AuthenticatorRequestDialogView::ManageDevicesButtonPressed() {
  sheet_->model()->OnManageDevices();
}

void AuthenticatorRequestDialogView::OnDialogClosing() {
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
  if (model_ && !model_->should_dialog_be_closed()) {
    Cancel();
  }
}

BEGIN_METADATA(AuthenticatorRequestDialogView, views::DialogDelegateView)
END_METADATA
