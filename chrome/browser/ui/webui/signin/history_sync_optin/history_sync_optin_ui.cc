// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/history_sync_optin/history_sync_optin_ui.h"

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin/history_sync_optin.mojom-data-view.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin/history_sync_optin_handler.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/signin_history_sync_optin_resources.h"
#include "chrome/grit/signin_history_sync_optin_resources_map.h"
#include "chrome/grit/signin_resources.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/base/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "net/base/url_util.h"
#include "ui/webui/webui_util.h"

namespace {
constexpr char kLaunchContextParamKey[] = "launch_context";

history_sync_optin::mojom::LaunchContext
GetHistorySyncOptinLaunchContextFromUrl(const GURL& url) {
  std::string launch_mode;
  // Default to window if the parameter is missing.
  history_sync_optin::mojom::LaunchContext launch_mode_enum =
      history_sync_optin::mojom::LaunchContext::kWindow;
  if (net::GetValueForKeyInQuery(url, kLaunchContextParamKey, &launch_mode)) {
    int launch_mode_int;
    base::StringToInt(launch_mode, &launch_mode_int);
    launch_mode_enum =
        static_cast<history_sync_optin::mojom::LaunchContext>(launch_mode_int);
  }
  return launch_mode_enum;
}

history_sync_optin::mojom::LaunchContext HistorySyncOptinLaunchContextToMojom(
    HistorySyncOptinLaunchContext launch_context) {
  switch (launch_context) {
    case HistorySyncOptinLaunchContext::kModal:
      return history_sync_optin::mojom::LaunchContext::kModal;
    case HistorySyncOptinLaunchContext::kWindow:
      return history_sync_optin::mojom::LaunchContext::kWindow;
  }
  NOTREACHED();
}

}  // namespace

bool HistorySyncOptinUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(
      syncer::kReplaceSyncPromosWithSignInPromos);
}

// static
GURL HistorySyncOptinUI::AppendHistorySyncOptinQueryParams(
    const GURL& url,
    HistorySyncOptinLaunchContext launch_context) {
  return net::AppendQueryParameter(
      url, kLaunchContextParamKey,
      base::NumberToString(static_cast<int>(
          HistorySyncOptinLaunchContextToMojom(launch_context))));
}

HistorySyncOptinUI::HistorySyncOptinUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, true),
      profile_(Profile::FromWebUI(web_ui)) {
  // Set up the chrome://history-sync-optin source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile_, chrome::kChromeUIHistorySyncOptinHost);

  // Add required resources.
  webui::SetupWebUIDataSource(
      source, base::span(kSigninHistorySyncOptinResources),
      IDR_SIGNIN_HISTORY_SYNC_OPTIN_HISTORY_SYNC_OPTIN_HTML);

  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"historySyncOptInTitle", IDS_HISTORY_SYNC_OPT_IN_TITLE},
      {"historySyncOptInSubtitle", IDS_HISTORY_SYNC_OPT_IN_SUBTITLE},
      {"historySyncOptInAcceptButtonLabel",
       IDS_HISTORY_SYNC_OPT_IN_ACCEPT_BUTTON},
      {"historySyncOptInRejectButtonLabel",
       IDS_HISTORY_SYNC_OPT_IN_REJECT_BUTTON},
      {"historySyncOptInDescription", IDS_HISTORY_SYNC_OPT_IN_DESCRIPTION},
  };

  source->AddResourcePath("images/window_left_illustration.svg",
                          IDR_SIGNIN_IMAGES_SHARED_LEFT_BANNER_SVG);
  source->AddResourcePath("images/window_left_illustration_dark.svg",
                          IDR_SIGNIN_IMAGES_SHARED_LEFT_BANNER_DARK_SVG);
  source->AddResourcePath("images/window_right_illustration.svg",
                          IDR_SIGNIN_IMAGES_SHARED_RIGHT_BANNER_SVG);
  source->AddResourcePath("images/window_right_illustration_dark.svg",
                          IDR_SIGNIN_IMAGES_SHARED_RIGHT_BANNER_DARK_SVG);

  source->AddLocalizedStrings(kLocalizedStrings);
  // Add avatar fallback value.
  source->AddString("accountPictureUrl",
                    profiles::GetPlaceholderAvatarIconUrl());

  source->AddInteger("launchContext",
                     static_cast<int>(GetHistorySyncOptinLaunchContextFromUrl(
                         web_ui->GetWebContents()->GetVisibleURL())));
}

HistorySyncOptinUI::~HistorySyncOptinUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(HistorySyncOptinUI)

void HistorySyncOptinUI::BindInterface(
    mojo::PendingReceiver<history_sync_optin::mojom::PageHandlerFactory>
        receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void HistorySyncOptinUI::Initialize(
    Browser* browser,
    std::optional<bool> should_close_modal_dialog,
    HistorySyncOptinHelper::FlowCompletedCallback
        history_optin_completed_callback) {
  // `browser` maybe null in the case of a window screen.
  // It must be set when the modal dialog is used.
  initialize_handler_callback_ = base::BindOnce(
      &HistorySyncOptinUI::OnMojoHandlersReady, weak_ptr_factory_.GetWeakPtr(),
      browser, should_close_modal_dialog,
      std::move(history_optin_completed_callback));
}

void HistorySyncOptinUI::CreateHistorySyncOptinHandler(
    mojo::PendingRemote<history_sync_optin::mojom::Page> page,
    mojo::PendingReceiver<history_sync_optin::mojom::PageHandler> receiver) {
  CHECK(page);
  CHECK(receiver);
  CHECK(initialize_handler_callback_);
  std::move(initialize_handler_callback_)
      .Run(std::move(page), std::move(receiver));
}

void HistorySyncOptinUI::OnMojoHandlersReady(
    Browser* browser,
    std::optional<bool> should_close_modal_dialog,
    HistorySyncOptinHelper::FlowCompletedCallback
        history_optin_completed_callback,
    mojo::PendingRemote<history_sync_optin::mojom::Page> page,
    mojo::PendingReceiver<history_sync_optin::mojom::PageHandler> receiver) {
  CHECK(!page_handler_);
  page_handler_ = std::make_unique<HistorySyncOptinHandler>(
      std::move(receiver), std::move(page), browser, profile_,
      should_close_modal_dialog, std::move(history_optin_completed_callback));
}
