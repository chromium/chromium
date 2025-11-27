// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signout_confirmation/signout_confirmation_ui.h"

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "chrome/browser/ui/webui/signin/signout_confirmation/signout_confirmation_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/signin_signout_confirmation_resources.h"
#include "chrome/grit/signin_signout_confirmation_resources_map.h"
#include "components/sync/base/features.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

SignoutConfirmationUI::SignoutConfirmationUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, true) {
  // Set up the chrome://signout-confirmation source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUISignoutConfirmationHost);

  source->AddBoolean("isUnoPhase2FollowUpEnabled",
                     base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp));

  webui::SetupWebUIDataSource(
      source, kSigninSignoutConfirmationResources,
      IDR_SIGNIN_SIGNOUT_CONFIRMATION_SIGNOUT_CONFIRMATION_HTML);

  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"unsyncedDataWithAccountExtensions",
       IDS_SIGNOUT_CONFIRMATION_UNSYNCED_DATA_WITH_ACCOUNT_EXTENSIONS},
  };
  source->AddLocalizedStrings(kLocalizedStrings);

  // Add a handler to provide pluralized strings.
  auto plural_string_handler = std::make_unique<PluralStringHandler>();
  plural_string_handler->AddLocalizedString(
      "extensionsSectionTitle",
      IDS_SIGNOUT_CONFIRMATION_EXTENSIONS_SECTION_TITLE);
  plural_string_handler->AddLocalizedString(
      "extensionsSectionTooltip",
      IDS_SIGNOUT_CONFIRMATION_EXTENSIONS_SECTION_TOOLTIP);

  web_ui->AddMessageHandler(std::move(plural_string_handler));
}

SignoutConfirmationUI::~SignoutConfirmationUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(SignoutConfirmationUI)

void SignoutConfirmationUI::Initialize(
    Browser* browser,
    ChromeSignoutConfirmationPromptVariant variant,
    size_t unsynced_data_count,
    SignoutConfirmationCallback callback) {
  initialize_handler_callback_ = base::BindOnce(
      &SignoutConfirmationUI::OnMojoHandlersReady, base::Unretained(this),
      browser, variant, unsynced_data_count, std::move(callback));
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
  handler_->Accept(/*uninstall_account_extensions=*/false);
}

void SignoutConfirmationUI::CancelDialogForTesting() {
  CHECK(handler_);
  handler_->Cancel(/*uninstall_account_extensions=*/false);
}

void SignoutConfirmationUI::CreateSignoutConfirmationHandler(
    mojo::PendingRemote<signout_confirmation::mojom::Page> page,
    mojo::PendingReceiver<signout_confirmation::mojom::PageHandler> receiver) {
  // Usually `Initialize()` is called right after loading the URL into the view.
  // However, some tests load the URL directly. In this case, populate the
  // handler with sample data.
  if (!initialize_handler_callback_) {
    CHECK_IS_TEST();
    Browser* browser = chrome::FindLastActive();
    Initialize(browser, ChromeSignoutConfirmationPromptVariant::kNoUnsyncedData,
               /*unsynced_data_count=*/0, base::DoNothing());
  }

  CHECK(initialize_handler_callback_);
  std::move(initialize_handler_callback_)
      .Run(std::move(page), std::move(receiver));
}

void SignoutConfirmationUI::OnMojoHandlersReady(
    Browser* browser,
    ChromeSignoutConfirmationPromptVariant variant,
    size_t unsynced_data_count,
    SignoutConfirmationCallback callback,
    mojo::PendingRemote<signout_confirmation::mojom::Page> page,
    mojo::PendingReceiver<signout_confirmation::mojom::PageHandler> receiver) {
  CHECK(!handler_);
  handler_ = std::make_unique<SignoutConfirmationHandler>(
      std::move(receiver), std::move(page), browser, variant,
      unsynced_data_count, std::move(callback));
}
