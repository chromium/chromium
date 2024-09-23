// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/nearby_share/nearby_share_dialog_ui.h"

#include <string>

#include "base/strings/string_split.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_manager.h"
#include "chrome/browser/nearby_sharing/file_attachment.h"
#include "chrome/browser/nearby_sharing/nearby_per_session_discovery_manager.h"
#include "chrome/browser/nearby_sharing/nearby_share_settings.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_impl.h"
#include "chrome/browser/nearby_sharing/text_attachment.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/nearby_share/shared_resources.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "chrome/browser/ui/webui/sanitized_image_source.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/nearby_share_dialog_resources.h"
#include "chrome/grit/nearby_share_dialog_resources_map.h"
#include "chrome/grit/theme_resources.h"
#include "chromeos/components/sharesheet/constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/url_util.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

namespace nearby_share {

// Keep in sync with //chrome/browser/resources/nearby_share/shared/types.js
enum class CloseReason {
  kUnknown = 0,
  kTransferStarted = 1,
  kTransferSucceeded = 2,
  kCancelled = 3,
  kRejected = 4,
  kMax = kRejected
};

bool NearbyShareDialogUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  if (browser_context->IsOffTheRecord())
    return false;
  return NearbySharingServiceFactory::IsNearbyShareSupportedForBrowserContext(
      browser_context);
}

NearbyShareDialogUI::NearbyShareDialogUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  Profile* profile = Profile::FromWebUI(web_ui);
  DCHECK(NearbySharingServiceFactory::IsNearbyShareSupportedForBrowserContext(
      profile));

  nearby_service_ = NearbySharingServiceFactory::GetForBrowserContext(profile);

  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(profile,
                                             chrome::kChromeUINearbyShareHost);

  content::URLDataSource::Add(profile,
                              std::make_unique<SanitizedImageSource>(profile));

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
      network::mojom::CSPDirectiveName::WorkerSrc,
      "worker-src blob: chrome://resources 'self';");

  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types static-types "
      // Required by lottie.
      "cros-lottie-worker-script-loader "
      "lottie-worker-script-loader webui-test-script "
      // Required by parse-html-subset.
      "parse-html-subset sanitize-inner-html "
      // Required by lit-html.
      "lit-html "
      // Required by polymer.
      "polymer-html-literal polymer-template-event-attribute-policy;");

  html_source->AddBoolean(
      "isOnePageOnboardingEnabled",
      base::FeatureList::IsEnabled(features::kNearbySharingOnePageOnboarding));
  RegisterNearbySharedStrings(html_source);
  html_source->UseStringsJs();

  // Register callback to handle "cancel-button-event" from nearby_*.html files.
  web_ui->RegisterMessageCallback(
      "close", base::BindRepeating(&NearbyShareDialogUI::HandleClose,
                                   base::Unretained(this)));

  auto plural_string_handler = std::make_unique<PluralStringHandler>();
  plural_string_handler->AddLocalizedString(
      "nearbyShareContactVisibilityNumUnreachable",
      IDS_NEARBY_CONTACT_VISIBILITY_NUM_UNREACHABLE_PH);
  web_ui->AddMessageHandler(std::move(plural_string_handler));
  // Add the metrics handler to write uma stats.
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());

  const GURL& url = web_ui->GetWebContents()->GetVisibleURL();
  SetAttachmentFromQueryParameter(url);
}

NearbyShareDialogUI::~NearbyShareDialogUI() = default;

void NearbyShareDialogUI::SetAttachments(
    std::vector<std::unique_ptr<Attachment>> attachments) {
  attachments_ = std::move(attachments);
}

void NearbyShareDialogUI::SetWebView(views::WebView* web_view) {
  CHECK(web_view);
  web_view_ = web_view;
  web_view_->GetWebContents()->SetDelegate(this);
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

void NearbyShareDialogUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

bool NearbyShareDialogUI::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  if (!web_view_) {
    return false;
  }

  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, web_view_->GetFocusManager());
}

void NearbyShareDialogUI::WebContentsCreated(
    content::WebContents* source_contents,
    int opener_render_process_id,
    int opener_render_frame_id,
    const std::string& frame_name,
    const GURL& target_url,
    content::WebContents* new_contents) {
  chrome::ScopedTabbedBrowserDisplayer displayer(Profile::FromWebUI(web_ui()));
  NavigateParams nav_params(displayer.browser(), target_url,
                            ui::PageTransition::PAGE_TRANSITION_LINK);
  Navigate(&nav_params);
}

void NearbyShareDialogUI::HandleClose(const base::Value::List& args) {
  if (!sharesheet_controller_)
    return;

  CHECK_EQ(1u, args.size());
  CHECK_GE(args[0].GetInt(), 0);
  CHECK_LE(args[0].GetInt(), static_cast<int>(CloseReason::kMax));
  CloseReason reason = static_cast<CloseReason>(args[0].GetInt());

  sharesheet::SharesheetResult sharesheet_result;
  switch (reason) {
    case CloseReason::kTransferStarted:
    case CloseReason::kTransferSucceeded:
      sharesheet_result = sharesheet::SharesheetResult::kSuccess;
      break;
    case CloseReason::kUnknown:
    case CloseReason::kCancelled:
    case CloseReason::kRejected:
      sharesheet_result = sharesheet::SharesheetResult::kCancel;
      break;
  }

  sharesheet_controller_->CloseBubble(sharesheet_result);

  // We need to clear out the controller here to protect against calling
  // CloseBubble() more than once, which will cause a crash.
  sharesheet_controller_ = nullptr;
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
          text_type.second, value, /*title=*/std::nullopt,
          /*mime_type=*/std::nullopt));
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
