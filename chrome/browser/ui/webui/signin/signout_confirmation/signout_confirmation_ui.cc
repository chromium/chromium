// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signout_confirmation/signout_confirmation_ui.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/signin/signout_confirmation/signout_confirmation_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/signin_signout_confirmation_resources.h"
#include "chrome/grit/signin_signout_confirmation_resources_map.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

SignoutConfirmationUI::SignoutConfirmationUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, false) {
  // Set up the chrome://signout-confirmation source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUISignoutConfirmationHost);

  // Currently, strings are added by the handler since they all depend on the
  // ChromeSignoutConfirmationPromptVariant.
  webui::SetupWebUIDataSource(
      source, kSigninSignoutConfirmationResources,
      IDR_SIGNIN_SIGNOUT_CONFIRMATION_SIGNOUT_CONFIRMATION_HTML);
}

SignoutConfirmationUI::~SignoutConfirmationUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(SignoutConfirmationUI)

void SignoutConfirmationUI::Initialize(
    Browser* browser,
    ChromeSignoutConfirmationPromptVariant variant,
    base::OnceCallback<void(ChromeSignoutConfirmationChoice)> callback) {
  initialize_handler_callback_ = base::BindOnce(
      &SignoutConfirmationUI::OnMojoHandlersReady, base::Unretained(this),
      browser, variant, std::move(callback));
}

void SignoutConfirmationUI::BindInterface(
    mojo::PendingReceiver<signout_confirmation::mojom::PageHandlerFactory>
        receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

// static
SignoutConfirmationUI* SignoutConfirmationUI::GetForTesting(
    content::WebContents* contents) {
  content::WebUI* web_ui = contents->GetWebUI();
  return web_ui ? web_ui->GetController()->GetAs<SignoutConfirmationUI>()
                : nullptr;
}

void SignoutConfirmationUI::AcceptDialogForTesting() {
  CHECK(handler_);
  handler_->Accept();
}

void SignoutConfirmationUI::CancelDialogForTesting() {
  CHECK(handler_);
  handler_->Cancel();
}

void SignoutConfirmationUI::CreateSignoutConfirmationHandler(
    mojo::PendingRemote<signout_confirmation::mojom::Page> page,
    mojo::PendingReceiver<signout_confirmation::mojom::PageHandler> receiver) {
  CHECK(initialize_handler_callback_);
  std::move(initialize_handler_callback_)
      .Run(std::move(page), std::move(receiver));
}

void SignoutConfirmationUI::OnMojoHandlersReady(
    Browser* browser,
    ChromeSignoutConfirmationPromptVariant variant,
    base::OnceCallback<void(ChromeSignoutConfirmationChoice)> callback,
    mojo::PendingRemote<signout_confirmation::mojom::Page> page,
    mojo::PendingReceiver<signout_confirmation::mojom::PageHandler> receiver) {
  CHECK(!handler_);
  handler_ = std::make_unique<SignoutConfirmationHandler>(
      std::move(receiver), std::move(page), browser, variant,
      std::move(callback));
}
