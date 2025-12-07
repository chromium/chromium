// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_request_dialog_view_controller_views.h"

#include <memory>
#include <utility>

#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/extensions/security_dialog_tracker.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_dialog_view.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/sheet_view_factory.h"
#include "chrome/browser/ui/webauthn/authenticator_request_dialog_view_controller.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "ui/views/widget/widget.h"

// static
std::unique_ptr<AuthenticatorRequestDialogViewController>
AuthenticatorRequestDialogViewController::Create(
    content::WebContents* web_contents,
    AuthenticatorRequestDialogModel* model) {
  // The authenticator request dialog will only be shown for common user-facing
  // WebContents, which have a |manager|. Most other sources without managers,
  // like service workers and extension background pages, do not allow WebAuthn
  // requests to be issued in the first place.
  // TODO(crbug.com/41392632): There are some niche WebContents where the
  // WebAuthn API is available, but there is no |manager| available. Currently,
  // we will not be able to show a dialog, so the |model| will be immediately
  // destroyed. The request may be able to still run to completion if it does
  // not require any user input, otherwise it will be blocked and time out. We
  // should audit this.
  auto* manager = web_modal::WebContentsModalDialogManager::FromWebContents(
      constrained_window::GetTopLevelWebContents(web_contents));
  if (!manager) {
    return nullptr;
  }

  return std::make_unique<AuthenticatorRequestDialogViewControllerViews>(
      web_contents, model);
}

AuthenticatorRequestDialogViewControllerViews::
    AuthenticatorRequestDialogViewControllerViews(
        content::WebContents* web_contents,
        AuthenticatorRequestDialogModel* model)
    : model_(model),
      view_(new AuthenticatorRequestDialogView(web_contents, model_)) {
  DCHECK(model_);
  DCHECK(!model_->should_dialog_be_closed());

  widget_ = constrained_window::ShowWebModalDialogViewsOwned(
      view_, web_contents, views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  extensions::SecurityDialogTracker::GetInstance()->AddSecurityDialog(
      widget_.get());

  observation_.Observe(model_);
}

AuthenticatorRequestDialogViewControllerViews::
    ~AuthenticatorRequestDialogViewControllerViews() = default;

void AuthenticatorRequestDialogViewControllerViews::OnStepTransition() {
  DCHECK(!model_->should_dialog_be_closed());
  view_->ReplaceCurrentSheetWith(CreateSheetViewForCurrentStepOf(model_));
  view_->Show();
}

void AuthenticatorRequestDialogViewControllerViews::OnSheetModelChanged() {
  view_->UpdateUIForCurrentSheet();
}

void AuthenticatorRequestDialogViewControllerViews::OnButtonsStateChanged() {
  view_->DialogModelChanged();
}
