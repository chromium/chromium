// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/ash/extended_updates/extended_updates_ui.h"

#include "ash/webui/common/trusted_types_util.h"
#include "base/containers/span.h"
#include "chrome/browser/ash/extended_updates/extended_updates_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/extended_updates/extended_updates.mojom.h"
#include "chrome/browser/ui/webui/ash/extended_updates/extended_updates_page_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/extended_updates_resources.h"
#include "chrome/grit/extended_updates_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

namespace ash::extended_updates {

ExtendedUpdatesUI::ExtendedUpdatesUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIExtendedUpdatesDialogHost);

  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"dialogHeading", IDS_EXTENDED_UPDATES_DIALOG_DIALOG_HEADING},
      {"dialogDescriptionP1",
       IDS_EXTENDED_UPDATES_DIALOG_DIALOG_DESCRIPTION_P1},
      {"cancelButton", IDS_EXTENDED_UPDATES_DIALOG_CANCEL_BUTTON},
      {"enableButton", IDS_EXTENDED_UPDATES_DIALOG_ENABLE_BUTTON},
      {"androidDescription", IDS_EXTENDED_UPDATES_DIALOG_ANDROID_DESCRIPTION},
      {"androidAppsListDescriptionSingular",
       IDS_EXTENDED_UPDATES_DIALOG_ANDROID_APPS_LIST_DESCRIPTION_SINGULAR},
      {"androidAppsListDescriptionPlural",
       IDS_EXTENDED_UPDATES_DIALOG_ANDROID_APPS_LIST_DESCRIPTION_PLURAL},
      {"androidAppsListNote",
       IDS_EXTENDED_UPDATES_DIALOG_ANDROID_APPS_LIST_NOTE},
      {"securityDescription", IDS_EXTENDED_UPDATES_DIALOG_SECURITY_DESCRIPTION},
      {"popupTitle", IDS_EXTENDED_UPDATES_DIALOG_POPUP_TITLE},
      {"popupConfirmButton", IDS_EXTENDED_UPDATES_DIALOG_POPUP_CONFIRM_BUTTON},
  };
  source->AddLocalizedStrings(kLocalizedStrings);

  source->AddString("dialogDescriptionP2",
                    l10n_util::GetStringFUTF16(
                        IDS_EXTENDED_UPDATES_DIALOG_DIALOG_DESCRIPTION_P2,
                        chrome::kDeviceExtendedUpdatesLearnMoreURL));
  source->AddString(
      "popupDescription",
      l10n_util::GetStringFUTF16(IDS_EXTENDED_UPDATES_DIALOG_POPUP_DESCRIPTION,
                                 ui::GetChromeOSDeviceName()));

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kExtendedUpdatesResources, kExtendedUpdatesResourcesSize),
      IDR_EXTENDED_UPDATES_EXTENDED_UPDATES_HTML);

  // For OOBE Adaptive Dialog.
  OobeUI::AddOobeComponents(source);

  ash::EnableTrustedTypesCSP(source);
}

ExtendedUpdatesUI::~ExtendedUpdatesUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(ExtendedUpdatesUI)

void ExtendedUpdatesUI::BindInterface(
    mojo::PendingReceiver<ash::extended_updates::mojom::PageHandlerFactory>
        receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void ExtendedUpdatesUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

void ExtendedUpdatesUI::CreatePageHandler(
    mojo::PendingRemote<ash::extended_updates::mojom::Page> page,
    mojo::PendingReceiver<ash::extended_updates::mojom::PageHandler> receiver) {
  DCHECK(page);
  page_handler_ = std::make_unique<ExtendedUpdatesPageHandler>(
      std::move(page), std::move(receiver), web_ui(),
      base::BindOnce(&ExtendedUpdatesUI::CloseDialog, base::Unretained(this),
                     base::Value::List()));
}

ExtendedUpdatesUIConfig::ExtendedUpdatesUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIExtendedUpdatesDialogHost) {}

ExtendedUpdatesUIConfig::~ExtendedUpdatesUIConfig() = default;

bool ExtendedUpdatesUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return ash::ExtendedUpdatesController::Get()->IsOptInEligible(
      browser_context);
}

}  // namespace ash::extended_updates
