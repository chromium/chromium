// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/authenticator_request_window.h"

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/trusted_vault/trusted_vault_encryption_keys_tab_helper.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

// Shows a top-level window containing some WebAuthn-related UI.
class AuthenticatorRequestWindow
    : public views::DialogDelegateView,
      public AuthenticatorRequestDialogModel::Observer {
 public:
  explicit AuthenticatorRequestWindow(AuthenticatorRequestDialogModel* model)
      : step_(model->current_step()), model_(model) {
    // Only one UI step involves showing a top-level window:
    CHECK_EQ(step_,
             AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);

    SetHasWindowSizeControls(true);
    SetCanResize(true);
    SetButtons(ui::DIALOG_BUTTON_NONE);
    set_use_custom_frame(false);
    SetUseDefaultFillLayout(true);
    SetShowCloseButton(true);
    SetShowTitle(true);
    SetTitle(u"Unlock Google Password Manager (UNTRANSLATED)");
    SetModalType(ui::MODAL_TYPE_NONE);

    SetCloseCallback(base::BindOnce(&AuthenticatorRequestWindow::OnClose,
                                    weak_ptr_factory_.GetWeakPtr()));

    auto web_view = std::make_unique<views::WebView>(
        model->GetRenderFrameHost()->GetBrowserContext());

    TrustedVaultEncryptionKeysTabHelper::CreateForWebContents(
        web_view->GetWebContents());
    web_view->RequestFocus();
    web_view->SetPreferredSize(gfx::Size(400, 700));
    // The kdi parameter here was generated from the following protobuf:
    //
    // {
    //   operation: RETRIEVAL
    //   retrieval_inputs: {
    //     security_domain_name: "hw_protected"
    //   }
    // }
    //
    // And then converted to bytes with:
    //
    // % gqui --outfile=rawproto:/tmp/out.pb from textproto:/tmp/input \
    //       proto gaia_frontend.ClientDecryptableKeyDataInputs
    //
    // Then the contents of `/tmp/out.pb` need to be base64url-encoded to
    // produce the "kdi" parameter's value.
    web_view->LoadInitialURL(
        GURL("https://accounts.google.com/encryption/unlock/"
             "desktop?kdi=CAESDgoMaHdfcHJvdGVjdGVk"));

    SetLayoutManager(std::make_unique<views::FillLayout>());
    AddChildView(std::move(web_view));
  }

  ~AuthenticatorRequestWindow() override {
    if (model_) {
      model_->RemoveObserver(this);
    }
  }

 protected:
  void OnClose() { model_->OnRecoverSecurityDomainClosed(); }

  // AuthenticatorRequestDialogModel::Observer:
  void OnModelDestroyed(AuthenticatorRequestDialogModel* model) override {
    model_ = nullptr;
  }

  void OnStepTransition() override {
    if (model_->current_step() != step_) {
      // Only one UI step involves a window so far. So any transition of the
      // model must be to a step that doesn't have one.
      GetWidget()->Close();
    }
  }

  void OnSheetModelChanged() override {}

 private:
  const AuthenticatorRequestDialogModel::Step step_;
  raw_ptr<AuthenticatorRequestDialogModel> model_;
  base::WeakPtrFactory<AuthenticatorRequestWindow> weak_ptr_factory_{this};
};

}  // namespace

void ShowAuthenticatorRequestWindow(AuthenticatorRequestDialogModel* model) {
  auto delegate = std::make_unique<AuthenticatorRequestWindow>(model);
  auto* delegate_ptr = delegate.get();
  views::DialogDelegate::CreateDialogWidget(std::move(delegate),
                                            /*context=*/nullptr,
                                            /*parent=*/nullptr);
  delegate_ptr->GetWidget()->Show();
}
