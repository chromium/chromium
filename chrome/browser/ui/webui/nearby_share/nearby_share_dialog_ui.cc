// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nearby_share/nearby_share_dialog_ui.h"

#include <string>

#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_manager.h"
#include "chrome/browser/nearby_sharing/nearby_per_session_discovery_manager.h"
#include "chrome/browser/nearby_sharing/nearby_share_settings.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/nearby_share/shared_resources.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/nearby_share_dialog_resources.h"
#include "chrome/grit/nearby_share_dialog_resources_map.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"

namespace nearby_share {

NearbyShareDialogUI::NearbyShareDialogUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  Profile* profile = Profile::FromWebUI(web_ui);
  // Nearby Share is not available to incognito or guest profiles.
  DCHECK(profile->IsRegularProfile());

  nearby_service_ = NearbySharingServiceFactory::GetForBrowserContext(profile);

  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUINearbyShareHost);

  webui::SetupWebUIDataSource(html_source,
                              base::make_span(kNearbyShareDialogResources,
                                              kNearbyShareDialogResourcesSize),
                              kNearbyShareGeneratedPath,
                              IDR_NEARBY_SHARE_NEARBY_SHARE_DIALOG_HTML);

  html_source->AddResourcePath("nearby_share.mojom-lite.js",
                               IDR_NEARBY_SHARE_MOJO_JS);
  html_source->AddResourcePath("nearby_share_target_types.mojom-lite.js",
                               IDR_NEARBY_SHARE_TARGET_TYPES_MOJO_JS);

  RegisterNearbySharedResources(html_source);
  RegisterNearbySharedStrings(html_source);
  html_source->UseStringsJs();

  web_ui->RegisterMessageCallback(
      "close", base::BindRepeating(&NearbyShareDialogUI::HandleClose,
                                   base::Unretained(this)));

  content::WebUIDataSource::Add(profile, html_source);
}

NearbyShareDialogUI::~NearbyShareDialogUI() = default;

void NearbyShareDialogUI::AddObserver(NearbyShareDialogUI::Observer* observer) {
  observers_.AddObserver(observer);
}

void NearbyShareDialogUI::RemoveObserver(
    NearbyShareDialogUI::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void NearbyShareDialogUI::SetAttachments(
    std::vector<std::unique_ptr<Attachment>> attachments) {
  attachments_ = std::move(attachments);
}

void NearbyShareDialogUI::BindInterface(
    mojo::PendingReceiver<mojom::DiscoveryManager> manager) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<NearbyPerSessionDiscoveryManager>(
          nearby_service_, std::move(attachments_)),
      std::move(manager));
}

void NearbyShareDialogUI::BindInterface(
    mojo::PendingReceiver<mojom::NearbyShareSettings> receiver) {
  NearbySharingService* nearby_sharing_service =
      NearbySharingServiceFactory::GetForBrowserContext(
          Profile::FromWebUI(web_ui()));
  nearby_sharing_service->GetSettings()->Bind(std::move(receiver));
}

void NearbyShareDialogUI::BindInterface(
    mojo::PendingReceiver<nearby_share::mojom::ContactManager> receiver) {
  NearbySharingService* nearby_sharing_service =
      NearbySharingServiceFactory::GetForBrowserContext(
          Profile::FromWebUI(web_ui()));
  nearby_sharing_service->GetContactManager()->Bind(std::move(receiver));
}

void NearbyShareDialogUI::HandleClose(const base::ListValue* args) {
  for (auto& observer : observers_) {
    observer.OnClose();
  }
}

WEB_UI_CONTROLLER_TYPE_IMPL(NearbyShareDialogUI)

}  // namespace nearby_share
