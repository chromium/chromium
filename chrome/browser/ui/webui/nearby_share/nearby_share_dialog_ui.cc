// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nearby_share/nearby_share_dialog_ui.h"

#include <string>

#include "base/strings/string_split.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_manager.h"
#include "chrome/browser/nearby_sharing/file_attachment.h"
#include "chrome/browser/nearby_sharing/nearby_per_session_discovery_manager.h"
#include "chrome/browser/nearby_sharing/nearby_share_settings.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_impl.h"
#include "chrome/browser/nearby_sharing/text_attachment.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/nearby_share/shared_resources.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/nearby_share_dialog_resources.h"
#include "chrome/grit/nearby_share_dialog_resources_map.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/url_util.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"

namespace nearby_share {

NearbyShareDialogUI::NearbyShareDialogUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  Profile* profile = Profile::FromWebUI(web_ui);
  DCHECK(NearbySharingServiceFactory::IsNearbyShareSupportedForBrowserContext(
      profile));

  nearby_service_ = NearbySharingServiceFactory::GetForBrowserContext(profile);

  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUINearbyShareHost);

  webui::SetupWebUIDataSource(html_source,
                              base::make_span(kNearbyShareDialogResources,
                                              kNearbyShareDialogResourcesSize),
                              IDR_NEARBY_SHARE_DIALOG_NEARBY_SHARE_DIALOG_HTML);

  // To use lottie, the worker-src CSP needs to be updated for the web ui that
  // is using it. Since as of now there are only a couple of webuis using
  // lottie animations, this update has to be performed manually. As the usage
  // increases, set this as the default so manual override is no longer
  // required.
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc, "worker-src blob: 'self';");

  RegisterNearbySharedStrings(html_source);
  html_source->UseStringsJs();

  web_ui->RegisterMessageCallback(
      "close", base::BindRepeating(&NearbyShareDialogUI::HandleClose,
                                   base::Unretained(this)));

  auto plural_string_handler = std::make_unique<PluralStringHandler>();
  plural_string_handler->AddLocalizedString(
      "nearbyShareContactVisibilityNumUnreachable",
      IDS_NEARBY_CONTACT_VISIBILITY_NUM_UNREACHABLE);
  web_ui->AddMessageHandler(std::move(plural_string_handler));
  // Add the metrics handler to write uma stats.
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());

  content::WebUIDataSource::Add(profile, html_source);

  const GURL& url = web_ui->GetWebContents()->GetVisibleURL();
  SetAttachmentFromQueryParameter(url);
}

NearbyShareDialogUI::~NearbyShareDialogUI() = default;

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
  if (sharesheet_controller_) {
    sharesheet_controller_->CloseSharesheet();

    // We need to clear out the controller here to protect against calling
    // CloseShareSheet() more than once, which will cause a crash.
    sharesheet_controller_ = nullptr;
  }
}

void NearbyShareDialogUI::SetAttachmentFromQueryParameter(const GURL& url) {
  std::vector<std::unique_ptr<Attachment>> attachments;
  std::string value;
  for (auto& text_type :
       std::vector<std::pair<std::string, TextAttachment::Type>>{
           {"address", TextAttachment::Type::kAddress},
           {"url", TextAttachment::Type::kUrl},
           {"phone", TextAttachment::Type::kPhoneNumber},
           {"text", TextAttachment::Type::kText}}) {
    if (net::GetValueForKeyInQuery(url, text_type.first, &value)) {
      attachments.push_back(std::make_unique<TextAttachment>(
          text_type.second, value, /*title=*/base::nullopt,
          /*mime_type=*/base::nullopt));
      SetAttachments(std::move(attachments));
      return;
    }
  }

  if (net::GetValueForKeyInQuery(url, "file", &value)) {
    for (std::string& path : base::SplitString(
             value, "|", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
      attachments.push_back(
          std::make_unique<FileAttachment>(base::FilePath(path)));
    }
    SetAttachments(std::move(attachments));
  }
}

WEB_UI_CONTROLLER_TYPE_IMPL(NearbyShareDialogUI)

}  // namespace nearby_share
