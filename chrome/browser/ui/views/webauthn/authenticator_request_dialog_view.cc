// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_request_dialog_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chrome/browser/ui/views/extensions/security_dialog_tracker.h"
#include "chrome/browser/ui/views/webauthn/authenticator_gpm_account_info_view.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/pin_options_button.h"
#include "chrome/browser/ui/views/webauthn/sheet_view_factory.h"
#include "chrome/browser/ui/webauthn/authenticator_request_sheet_model.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"

using Step = AuthenticatorRequestDialogModel::Step;

// static
void ShowAuthenticatorRequestDialog(
    content::WebContents* web_contents,
    scoped_refptr<AuthenticatorRequestDialogModel> model) {
  // The authenticator request dialog will only be shown for common user-facing
  // WebContents, which have a |manager|. Most other sources without managers,
  // like service workers and extension background pages, do not allow WebAuthn
  // requests to be issued in the first place.
  // TODO(crbug.com/41392632): There are some niche WebContents where the
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

  new AuthenticatorRequestDialogView(web_contents, std::move(model));
}

AuthenticatorRequestDialogView::~AuthenticatorRequestDialogView() {
  model_->observers.RemoveObserver(this);
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

  int buttons = static_cast<int>(ui::mojom::DialogButton::kNone);
  if (sheet()->model()->IsAcceptButtonVisible()) {
    buttons |= static_cast<int>(ui::mojom::DialogButton::kOk);
  }
  if (sheet()->model()->IsCancelButtonVisible()) {
    buttons |= static_cast<int>(ui::mojom::DialogButton::kCancel);
  }
  SetButtons(buttons);
  SetDefaultButton(buttons & static_cast<int>(ui::mojom::DialogButton::kOk)
                       ? static_cast<int>(ui::mojom::DialogButton::kOk)
                       : static_cast<int>(ui::mojom::DialogButton::kNone));
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 sheet_->model()->GetAcceptButtonLabel());
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 sheet_->model()->GetCancelButtonLabel());
  if (model_->step() == Step::kTrustThisComputerAssertion ||
      model_->step() == Step::kTrustThisComputerCreation ||
      model_->step() == Step::kGPMCreatePasskey ||
      model_->step() == Step::kGPMEnterPin ||
      model_->step() == Step::kGPMEnterArbitraryPin ||
      model_->step() == Step::kGPMCreatePin ||
      model_->step() == Step::kGPMCreateArbitraryPin ||
      model_->step() == Step::kGPMChangePin ||
      model_->step() == Step::kGPMChangeArbitraryPin) {
    SetButtonStyle(ui::mojom::DialogButton::kCancel, ui::ButtonStyle::kTonal);
  }

  if (ShouldOtherMechanismsButtonBeVisible()) {
    auto* other_mechanisms = SetExtraView(std::make_unique<views::MdTextButton>(
        base::BindRepeating(
            &AuthenticatorRequestDialogView::OtherMechanismsButtonPressed,
            base::Unretained(this)),
        sheet_->model()->GetOtherMechanismButtonLabel()));
    other_mechanisms->SetEnabled(!model_->ui_disabled_);
  } else if (sheet_->model()->IsManageDevicesButtonVisible()) {
    auto* manage_devices = SetExtraView(std::make_unique<views::MdTextButton>(
        base::BindRepeating(
            &AuthenticatorRequestDialogView::ManageDevicesButtonPressed,
            base::Unretained(this)),
        l10n_util::GetStringUTF16(IDS_WEBAUTHN_MANAGE_DEVICES)));
    manage_devices->SetEnabled(!model_->ui_disabled_);
  } else if (sheet_->model()->IsForgotGPMPinButtonVisible()) {
    auto forgot_pin_button = std::make_unique<views::MdTextButton>(
        base::BindRepeating(
            &AuthenticatorRequestDialogView::ForgotGPMPinPressed,
            base::Unretained(this)),
        l10n_util::GetStringUTF16(IDS_WEBAUTHN_FORGOT_GPM_PIN_BUTTON));
    forgot_pin_button->SetEnabled(!model_->ui_disabled_);
    SetExtraView(std::move(forgot_pin_button));
  } else if (sheet_->model()->IsGPMPinOptionsButtonVisible()) {
    PinOptionsButton::CommandId checked_command_id =
        (model_->step() == Step::kGPMCreateArbitraryPin ||
         model_->step() == Step::kGPMChangeArbitraryPin)
            ? PinOptionsButton::CommandId::CHOOSE_ARBITRARY_PIN
            : PinOptionsButton::CommandId::CHOOSE_SIX_DIGIT_PIN;
    auto pin_options_button = std::make_unique<PinOptionsButton>(
        l10n_util::GetStringUTF16(IDS_WEBAUTHN_GPM_PIN_OPTIONS_BUTTON),
        checked_command_id,
        base::BindRepeating(&AuthenticatorRequestDialogView::GPMPinOptionChosen,
                            base::Unretained(this)));
    pin_options_button->SetEnabled(!model_->ui_disabled_);
    SetExtraView(std::move(pin_options_button));
  } else {
    SetExtraView(std::make_unique<views::View>());
  }

  DialogModelChanged();

  // If the widget is not yet shown or already being torn down, we are done. In
  // the former case, sizing/layout will happen once the dialog is visible.
  if (!GetWidget()) {
    return;
  }

  UpdateFooter();

  // Force re-layout of the entire dialog client view, which includes the sheet
  // content as well as the button row on the bottom.
  // TODO(ellyjones): Why is this necessary?
  GetWidget()->GetRootView()->DeprecatedLayoutImmediately();

  // The accessibility title is also sourced from the |sheet_|'s step title.
  GetWidget()->UpdateWindowTitle();

  // TODO(crbug.com/41392632): Investigate how a web-modal dialog's
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
  if (model_->ui_disabled_ && sheet_->model()->IsActivityIndicatorVisible()) {
    // Announce the loading state after request focus; otherwise the view that
    // has the focus will suppress the loading announcement.
    GetViewAccessibility().AnnounceText(
        l10n_util::GetStringUTF16(IDS_WEBAUTHN_LOADING));
  }
}

bool AuthenticatorRequestDialogView::ShouldOtherMechanismsButtonBeVisible()
    const {
  return sheet_->model()->IsOtherMechanismButtonVisible();
}

void AuthenticatorRequestDialogView::AddedToWidget() {
  // Updating footer requires widget to be present.
  UpdateFooter();
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
    ui::mojom::DialogButton button) const {
  switch (button) {
    case ui::mojom::DialogButton::kNone:
      break;
    case ui::mojom::DialogButton::kOk:
      return sheet()->model()->IsAcceptButtonEnabled();
    case ui::mojom::DialogButton::kCancel:
      return true;  // Cancel is always enabled if visible.
  }
  NOTREACHED();
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
    return GetExtraView();
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
  NOTREACHED() << "The model should outlive this view.";
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
  ReplaceCurrentSheetWith(CreateSheetViewForCurrentStepOf(model_.get()));
  Show();
}

void AuthenticatorRequestDialogView::OnSheetModelChanged() {
  UpdateUIForCurrentSheet();
}

void AuthenticatorRequestDialogView::OnButtonsStateChanged() {
  DialogModelChanged();
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
    scoped_refptr<AuthenticatorRequestDialogModel> model)
    : content::WebContentsObserver(web_contents),
      model_(model),
      web_contents_hidden_(web_contents->GetVisibility() ==
                           content::Visibility::HIDDEN) {
  SetShowTitle(false);
  DCHECK(!model_->should_dialog_be_closed());
  model_->observers.AddObserver(this);

  SetModalType(ui::mojom::ModalType::kChild);
  SetShowCloseButton(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  // Currently, all sheets have a label on top and controls at the bottom.
  // Consider moving this to AuthenticatorRequestSheetView if this changes.
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

void AuthenticatorRequestDialogView::Show() {
  if (!first_shown_) {
    views::Widget* widget =
        constrained_window::ShowWebModalDialogViews(this, web_contents());
    DCHECK(widget);
    extensions::SecurityDialogTracker::GetInstance()->AddSecurityDialog(widget);
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

void AuthenticatorRequestDialogView::ForgotGPMPinPressed() {
  sheet_->model()->OnForgotGPMPin();
}

void AuthenticatorRequestDialogView::GPMPinOptionChosen(bool is_arbitrary) {
  sheet_->model()->OnGPMPinOptionChosen(is_arbitrary);
}

void AuthenticatorRequestDialogView::UpdateFooter() {
  if (!GetWidget()) {
    return;
  }

  auto* frame_view = GetBubbleFrameView();
  if (model_->step() == Step::kGPMCreatePin ||
      model_->step() == Step::kGPMCreateArbitraryPin ||
      model_->step() == Step::kGPMChangePin ||
      model_->step() == Step::kGPMChangeArbitraryPin ||
      model_->step() == Step::kGPMEnterPin ||
      model_->step() == Step::kGPMEnterArbitraryPin) {
    frame_view->SetFootnoteView(
        std::make_unique<AuthenticatorGpmAccountInfoView>(
            static_cast<AuthenticatorGpmPinSheetModelBase*>(sheet_->model())));
  } else {
    frame_view->SetFootnoteView(nullptr);
  }
}

BEGIN_METADATA(AuthenticatorRequestDialogView)
END_METADATA
