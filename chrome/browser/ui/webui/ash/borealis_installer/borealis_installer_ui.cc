// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/borealis_installer/borealis_installer_ui.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/borealis_installer_resources.h"
#include "chrome/grit/borealis_installer_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace ash {

bool BorealisInstallerUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(features::kBorealisWebUIInstaller);
}

BorealisInstallerUI::BorealisInstallerUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI{web_ui}, web_ui_(web_ui) {
  // Set up the chrome://borealis-installer source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          chrome::kChromeUIBorealisInstallerHost);
  static constexpr webui::LocalizedString kStrings[] = {
      {"cancel", IDS_CANCEL},
      {"install", IDS_INSTALL},
      {"confirmationTitle", IDS_BOREALIS_INSTALLER_CONFIRMATION_TITLE},
      {"confirmationMessage", IDS_BOREALIS_INSTALLER_CONFIRMATION_MESSAGE},
      {"ongoingTitle", IDS_BOREALIS_INSTALLER_ONGOING_TITLE},
      {"ongingMessage", IDS_BOREALIS_INSTALLER_ONGOING_MESSAGE},
      {"percent", IDS_BOREALIS_INSTALLER_ONGOING_PERCENTAGE},
      {"finishedTitle", IDS_BOREALIS_INSTALLER_FINISHED_TITLE},
      {"finishedMessage", IDS_BOREALIS_INSTALLER_FINISHED_MESSAGE},
      {"launch", IDS_BOREALIS_INSTALLER_LAUNCH_BUTTON},
  };
  html_source->AddLocalizedStrings(kStrings);

  webui::SetupWebUIDataSource(html_source,
                              base::make_span(kBorealisInstallerResources,
                                              kBorealisInstallerResourcesSize),
                              IDR_BOREALIS_INSTALLER_BOREALIS_INSTALLER_HTML);
}

BorealisInstallerUI::~BorealisInstallerUI() = default;

void BorealisInstallerUI::BindPageHandlerFactory(
    mojo::PendingReceiver<ash::borealis_installer::mojom::PageHandlerFactory>
        pending_receiver) {
  if (page_factory_receiver_.is_bound()) {
    page_factory_receiver_.reset();
  }

  page_factory_receiver_.Bind(std::move(pending_receiver));
}

void BorealisInstallerUI::BindInterface(
    mojo::PendingReceiver<borealis_installer::mojom::PageHandlerFactory>
        pending_receiver) {
  if (page_factory_receiver_.is_bound()) {
    page_factory_receiver_.reset();
  }

  page_factory_receiver_.Bind(std::move(pending_receiver));
}

void BorealisInstallerUI::CreatePageHandler(
    mojo::PendingRemote<ash::borealis_installer::mojom::Page> pending_page,
    mojo::PendingReceiver<ash::borealis_installer::mojom::PageHandler>
        pending_page_handler) {
  DCHECK(pending_page.is_valid());

  page_handler_ = std::make_unique<BorealisInstallerPageHandler>(
      std::move(pending_page_handler), std::move(pending_page),
      base::BindOnce(&BorealisInstallerUI::OnPageClosed,
                     base::Unretained(this)),
      web_ui_);
}

void BorealisInstallerUI::OnPageClosed() {
  page_closed_ = true;
  // CloseDialog() is a no-op if we are not in a dialog (e.g. user
  // access the page using the URL directly, which is not supported).
  ui::MojoWebDialogUI::CloseDialog(base::Value::List());
}

WEB_UI_CONTROLLER_TYPE_IMPL(BorealisInstallerUI);

}  // namespace ash
