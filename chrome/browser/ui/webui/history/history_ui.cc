// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history/history_ui.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/page_image_service/image_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/cr_components/history/history_util.h"
#include "chrome/browser/ui/webui/history/browsing_history_handler.h"
#include "chrome/browser/ui/webui/history/history_login_handler.h"
#include "chrome/browser/ui/webui/history/history_ui_platform_handlers.h"
#include "chrome/browser/ui/webui/history/navigation_handler.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/history_resources.h"
#include "chrome/grit/history_resources_map.h"
#include "components/critical_actions/core/browser/features.h"
#include "components/grit/components_scaled_resources.h"
#include "components/history/core/browser/features.h"
#include "components/page_image_service/image_service.h"
#include "components/page_image_service/image_service_handler.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/webui/tracked_element/tracked_element_handler_document_singleton.h"
#include "ui/webui/webui_util.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/cr_components/history_embeddings/history_embeddings_handler.h"
#include "chrome/browser/ui/webui/history_clusters/history_clusters_handler.h"
#include "chrome/browser/ui/webui/page_not_available_for_guest/page_not_available_for_guest_ui.h"  // nogncheck
#include "components/tabs/public/tab_interface.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace {

content::WebUIDataSource* CreateAndAddHistoryUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIHistoryHost);

  source->AddBoolean("replaceSyncPromosWithSignInPromos",
                     syncer::IsReplaceSyncPromosWithSignInPromosEnabled());

#if !BUILDFLAG(IS_CHROMEOS)
  source->AddBoolean("unoPhase2FollowUp",
                     base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp));
#endif  // !BUILDFLAG(IS_CHROMEOS)

  HistoryUtil::PopulateCommonSourceForHistory(source, profile);

  static constexpr webui::LocalizedString kStrings[] = {
      // Localized strings (alphabetical order).
      {"compareHistoryEmpty", IDS_COMPARE_HISTORY_EMPTY},
      {"compareHistoryRemove", IDS_COMPARE_HISTORY_REMOVE},
      {"compareHistoryHeader", IDS_COMPARE_HISTORY_HEADER},
      {"compareHistoryInfo", IDS_COMPARE_HISTORY_INFO},
      {"compareHistoryListsMenuItem", IDS_COMPARE_HISTORY_MENU_ITEM},
      {"compareHistoryRow", IDS_COMPARE_HISTORY_ROW},
      {"compareHistoryMenuAriaLabel", IDS_COMPARE_HISTORY_MENU_ARIA_LABEL},
      {"noSyncedResults", IDS_HISTORY_NO_SYNCED_RESULTS},
      {"signinOnPhonePromoButton", IDS_HISTORY_SIGNIN_ON_PHONE_PROMO_BUTTON},
      {"signinOnPhonePromoSubtitle",
       IDS_HISTORY_SIGNIN_ON_PHONE_PROMO_SUBTITLE},
      {"signinOnPhonePromoTitle", IDS_HISTORY_SIGNIN_ON_PHONE_PROMO_TITLE},
      {"turnOnSyncPromo", IDS_HISTORY_TURN_ON_SYNC_PROMO},
      {"turnOnSyncPromoDesc", IDS_HISTORY_TURN_ON_SYNC_PROMO_DESC},
      {"turnOnSyncHistoryPromo", IDS_HISTORY_SYNC_HISTORY_PROMO},
      {"syncHistoryPromoBodySignedOut",
       IDS_RECENT_TABS_SYNC_HISTORY_PROMO_BODY_SIGNED_OUT},
      {"syncHistoryPromoBodyPendingSignIn",
       IDS_RECENT_TABS_SYNC_HISTORY_PROMO_BODY_PENDING_SIGN_IN},
      {"syncHistoryPromoBodyPendingSignInSyncHistoryOn",
       IDS_RECENT_TABS_SYNC_HISTORY_PROMO_BODY_PENDING_SIGN_IN_SYNC_HISTORY_ON},
      {"verifyItsYou", IDS_VERIFY_IT_IS_YOU}};
  source->AddLocalizedStrings(kStrings);

  source->AddLocalizedString("turnOnSyncHistoryButton",
                             IDS_HISTORY_SYNC_HISTORY_BUTTON);
  source->AddString("accountPictureUrl",
                    profiles::GetPlaceholderAvatarIconUrl());

  const bool is_critical_actions_enabled = base::FeatureList::IsEnabled(
      critical_actions::features::kCriticalActionHistory);
  const bool is_critical_actions_chat_linkouts_enabled =
      is_critical_actions_enabled &&
      critical_actions::features::kEnableChatLinkouts.Get();

  source->AddString(
      "sidebarFooterGMAOnly",
      l10n_util::GetStringFUTF16(IDS_HISTORY_OTHER_FORMS_OF_HISTORY_GMA_ONLY,
                                 chrome::kMyActivityUrlInHistory));
  source->AddString(
      "sidebarFooterGAAOnly",
      l10n_util::GetStringFUTF16(IDS_HISTORY_OTHER_FORMS_OF_HISTORY_GAA_ONLY,
                                 chrome::kMyActivityGeminiAppsUrl));
  source->AddString(
      "sidebarFooterGMAAndGAA",
      l10n_util::GetStringFUTF16(
          is_critical_actions_enabled
              ? IDS_HISTORY_OTHER_FORMS_OF_HISTORY_GMA_AND_GAA_CRITICAL_ACTIONS
              : IDS_HISTORY_OTHER_FORMS_OF_HISTORY_GMA_AND_GAA,
          chrome::kMyActivityUrlInHistory, chrome::kMyActivityGeminiAppsUrl));
  source->AddString("sidebarFooterGMALink", chrome::kMyActivityUrlInHistory);
  source->AddString("sidebarFooterGAALink", chrome::kMyActivityGeminiAppsUrl);

#if !BUILDFLAG(IS_CHROMEOS)
  static constexpr webui::LocalizedString kHistorySyncStrings[] = {
      {"historySyncPromoTitle", IDS_HISTORY_SYNC_PROMO_TITLE},
      {"historySyncPromoBodySignedOut", IDS_HISTORY_SYNC_PROMO_BODY_SIGNED_OUT},
      {"historySyncPromoBodySignInPending",
       IDS_HISTORY_SYNC_PROMO_BODY_SIGN_IN_PENDING},
      {"historySyncPromoBodySignInPendingSyncHistoryOn",
       IDS_HISTORY_SYNC_PROMO_BODY_SIGN_IN_PENDING_SYNC_HISTORY_ON},
  };
  source->AddLocalizedStrings(kHistorySyncStrings);
#endif  // !BUILDFLAG(IS_CHROMEOS)

  source->AddBoolean("isBrowsingHistoryActorIntegrationM3Enabled",
                     history::IsBrowsingHistoryActorIntegrationM3Enabled());
  source->AddBoolean("isCriticalActionsEnabled", is_critical_actions_enabled);
  source->AddBoolean("isCriticalActionsChatLinkoutsEnabled",
                     is_critical_actions_chat_linkouts_enabled);

  source->AddString("webuiRefresh2026", features::IsWebuiRefresh2026Enabled()
                                            ? "webui-refresh-2026"
                                            : "");

  // Platform-specific data sources (e.g. history clusters, embeddings).
  history::PopulatePlatformDataSource(source, profile);

  return source;
}

}  // namespace

HistoryUIConfig::HistoryUIConfig()
    : WebUIConfig(content::kChromeUIScheme, chrome::kChromeUIHistoryHost) {}

HistoryUIConfig::~HistoryUIConfig() = default;

std::unique_ptr<content::WebUIController>
HistoryUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                       const GURL& url) {
#if !BUILDFLAG(IS_ANDROID)
  Profile* profile = Profile::FromWebUI(web_ui);
  if (profile->IsGuestSession()) {
    return std::make_unique<PageNotAvailableForGuestUI>(
        web_ui, chrome::kChromeUIHistoryHost);
  }
#endif  // !BUILDFLAG(IS_ANDROID)
  return std::make_unique<HistoryUI>(web_ui);
}

HistoryUI::HistoryUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* data_source =
      CreateAndAddHistoryUIHTMLSource(profile);
  history::InitializePlatformHandlers(web_ui, data_source);
  history::InitializePlatformRegistrars(
      pref_change_registrar_, profile,
      base::BindRepeating(&HistoryUI::UpdateDataSource,
                          base::Unretained(this)));

  web_ui->AddMessageHandler(std::make_unique<webui::NavigationHandler>());
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());
  web_ui->AddMessageHandler(
      std::make_unique<HistoryLoginHandler>(base::BindRepeating(
          &HistoryUI::UpdateDataSource, base::Unretained(this))));

  ui::TrackedElementHandlerDocumentSingleton::Register(
      this,
      std::vector<ui::ElementIdentifier>{kHistorySearchInputElementId,
                                         kHistoryGeminiFilterChipElementId});
}

HistoryUI::~HistoryUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(HistoryUI)

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(HistoryUI,
                                      kHistoryGeminiFilterChipElementId);

// static
scoped_refptr<base::RefCountedMemory> HistoryUI::GetFaviconResourceBytes(
    ui::ResourceScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_HISTORY_FAVICON, scale_factor);
}

#if !BUILDFLAG(IS_ANDROID)
void HistoryUI::BindInterface(
    mojo::PendingReceiver<history_embeddings::mojom::PageHandlerFactory>
        pending_page_handler_factory) {
  history_embeddings_handler_factory_receiver_.reset();
  history_embeddings_handler_factory_receiver_.Bind(
      std::move(pending_page_handler_factory));
}

void HistoryUI::CreatePageHandler(
    mojo::PendingRemote<history_embeddings::mojom::Page> page,
    mojo::PendingReceiver<history_embeddings::mojom::PageHandler> receiver) {
  history_embeddings_handler_ = std::make_unique<HistoryEmbeddingsHandler>(
      std::move(receiver), std::move(page),
      Profile::FromWebUI(web_ui())->GetWeakPtr(), web_ui(),
      /*for_side_panel=*/false);
}

void HistoryUI::BindInterface(
    mojo::PendingReceiver<history_clusters::mojom::PageHandlerFactory>
        pending_page_handler_factory) {
  history_clusters_handler_factory_receiver_.reset();
  history_clusters_handler_factory_receiver_.Bind(
      std::move(pending_page_handler_factory));
}

void HistoryUI::CreatePageHandler(
    mojo::PendingRemote<history_clusters::mojom::Page> page,
    mojo::PendingReceiver<history_clusters::mojom::PageHandler> receiver) {
  history_clusters_handler_ =
      std::make_unique<history_clusters::HistoryClustersHandler>(
          std::move(receiver), std::move(page), Profile::FromWebUI(web_ui()),
          web_ui()->GetWebContents(),
          // HistoryUI should always be in a tab. Look it up unconditionally.
          tabs::TabInterface::GetFromContents(web_ui()->GetWebContents()));
}
#endif  // !BUILDFLAG(IS_ANDROID)

void HistoryUI::BindInterface(
    mojo::PendingReceiver<history::mojom::PageHandler> pending_page_handler) {
  browsing_history_handler_ = std::make_unique<BrowsingHistoryHandler>(
      std::move(pending_page_handler), Profile::FromWebUI(web_ui()),
      web_ui()->GetWebContents());
}

void HistoryUI::BindInterface(
    mojo::PendingReceiver<history_cross_device_signin_promo::mojom::
                              HistoryCrossDeviceSigninPromoHandler>
        pending_receiver) {
  history_cross_device_signin_promo_handler_ =
      history::CreateCrossDeviceSigninPromoHandler(std::move(pending_receiver),
                                                   web_ui());
}

void HistoryUI::BindInterface(
    mojo::PendingReceiver<history::mojom::ForeignSessionPageHandlerFactory>
        pending_receiver) {
  foreign_session_page_handler_factory_receiver_.reset();
  foreign_session_page_handler_factory_receiver_.Bind(
      std::move(pending_receiver));
}

void HistoryUI::CreateForeignSessionPageHandler(
    mojo::PendingRemote<history::mojom::ForeignSessionPage> page,
    mojo::PendingReceiver<history::mojom::ForeignSessionPageHandler> receiver) {
  foreign_session_handler_ = history::CreateForeignSessionPageHandler(
      std::move(page), std::move(receiver), web_ui());
}

void HistoryUI::BindInterface(
    mojo::PendingReceiver<page_image_service::mojom::PageImageServiceHandler>
        pending_page_handler) {
  base::WeakPtr<page_image_service::ImageService> image_service_weak;
  if (auto* image_service =
          page_image_service::ImageServiceFactory::GetForBrowserContext(
              Profile::FromWebUI(web_ui()))) {
    image_service_weak = image_service->GetWeakPtr();
  }
  image_service_handler_ =
      std::make_unique<page_image_service::ImageServiceHandler>(
          std::move(pending_page_handler), std::move(image_service_weak));
}

void HistoryUI::UpdateDataSource() {
  CHECK(web_ui());
  history::UpdatePlatformDataSource(web_ui());
}

void HistoryUI::BindInterface(
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
        pending_receiver) {
  if (help_bubble_handler_factory_receiver_.is_bound()) {
    help_bubble_handler_factory_receiver_.reset();
  }
  help_bubble_handler_factory_receiver_.Bind(std::move(pending_receiver));
}

void HistoryUI::CreateHelpBubbleHandler(
    mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> client,
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler> handler) {
  help_bubble_handler_ = std::make_unique<user_education::HelpBubbleHandler>(
      std::move(handler), std::move(client),
      ui::TrackedElementHandlerDocumentSingleton::GetOrCreate(
          web_ui()->GetRenderFrameHost()));
}

void HistoryUI::BindInterface(
    mojo::PendingReceiver<
        user_education::mojom::UserEducationMixedTrustHandlerFactory>
        pending_receiver) {
  user_education_handler_factory_receiver_.reset();
  user_education_handler_factory_receiver_.Bind(std::move(pending_receiver));
}

void HistoryUI::CreateUserEducationMixedTrustHandler(
    mojo::PendingReceiver<user_education::mojom::UserEducationMixedTrustHandler>
        receiver) {
  user_education_handler_ = history::CreateUserEducationMixedTrustHandler(
      std::move(receiver), web_ui());
}
