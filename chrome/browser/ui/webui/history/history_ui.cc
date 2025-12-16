// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history/history_ui.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history_embeddings/history_embeddings_utils.h"
#include "chrome/browser/page_image_service/image_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/cr_components/history/history_util.h"
#include "chrome/browser/ui/webui/cr_components/history_clusters/history_clusters_util.h"
#include "chrome/browser/ui/webui/cr_components/history_embeddings/history_embeddings_handler.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/history/browsing_history_handler.h"
#include "chrome/browser/ui/webui/history/foreign_session_handler.h"
#include "chrome/browser/ui/webui/history/history_identity_state_watcher.h"
#include "chrome/browser/ui/webui/history/history_login_handler.h"
#include "chrome/browser/ui/webui/history/navigation_handler.h"
#include "chrome/browser/ui/webui/history_clusters/history_clusters_handler.h"
#include "chrome/browser/ui/webui/managed_ui_handler.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/page_not_available_for_guest/page_not_available_for_guest_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/history_resources.h"
#include "chrome/grit/history_resources_map.h"
#include "components/grit/components_scaled_resources.h"
#include "components/history/core/common/pref_names.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/history_clusters_prefs.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/page_image_service/image_service.h"
#include "components/page_image_service/image_service_handler.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/features.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/webui/webui_util.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/public/glic_enabling.h"
#endif

namespace {

content::WebUIDataSource* CreateAndAddHistoryUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIHistoryHost);

  source->AddBoolean(
      "replaceSyncPromosWithSignInPromos",
      base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos));

#if !BUILDFLAG(IS_CHROMEOS)
  source->AddBoolean("unoPhase2FollowUp",
                     base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp));
#endif  // BUILDFLAG!(IS_CHROMEOS)

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

  // The history page footer can display messages about other forms of
  // browsing history, linking to Google My Activity (GMA) and/or
  // Gemini Apps Activity (GAA). At most one message is shown, depending on
  // the user's settings.
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
      l10n_util::GetStringFUTF16(IDS_HISTORY_OTHER_FORMS_OF_HISTORY_GMA_AND_GAA,
                                 chrome::kMyActivityUrlInHistory,
                                 chrome::kMyActivityGeminiAppsUrl));
  // Links that are used in the messages above.
  source->AddString("sidebarFooterGMALink", chrome::kMyActivityUrlInHistory);
  source->AddString("sidebarFooterGAALink", chrome::kMyActivityGeminiAppsUrl);

#if BUILDFLAG(ENABLE_GLIC)
  const bool is_glic_enabled =
      glic::GlicEnabling::ShouldShowSettingsPage(profile);
#else
  const bool is_glic_enabled = false;
#endif  // BUILDFLAG(ENABLE_GLIC)

  source->AddBoolean("isGlicEnabled", is_glic_enabled);

#if BUILDFLAG(IS_CHROMEOS)
  source->AddLocalizedString("turnOnSyncButton",
                             IDS_HISTORY_TURN_ON_SYNC_BUTTON);
#else
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  bool has_primary_account =
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin);
  AccountInfo account_info =
      signin_ui_util::GetSingleAccountForPromos(identity_manager);
  source->AddString(
      "historySyncPromoBodySignedIn",
      l10n_util::GetStringFUTF16(IDS_HISTORY_SYNC_PROMO_BODY_SIGNED_IN,
                                 base::UTF8ToUTF16(account_info.email)));
  source->AddString(
      "turnOnSignedInSyncHistoryPromoBodySignInSyncOff",
      l10n_util::GetStringFUTF16(
          IDS_RECENT_TABS_SYNC_HISTORY_PROMO_BODY_SIGNED_IN_SYNC_OFF,
          base::UTF8ToUTF16(account_info.email)));
  source->AddString("accountName", account_info.full_name);
  source->AddString("accountEmail", account_info.email);
  if (!has_primary_account && !account_info.IsEmpty()) {
    source->AddString("turnOnSyncButton",
                      l10n_util::GetStringFUTF16(
                          IDS_PROFILES_DICE_WEB_ONLY_SIGNIN_BUTTON,
                          base::UTF8ToUTF16(!account_info.given_name.empty()
                                                ? account_info.given_name
                                                : account_info.email)));
  } else {
    source->AddLocalizedString("turnOnSyncButton",
                               IDS_HISTORY_TURN_ON_SYNC_BUTTON);
  }

  static constexpr webui::LocalizedString kHistorySyncStrings[] = {
      {"historySyncPromoTitle", IDS_HISTORY_SYNC_PROMO_TITLE},
      {"historySyncPromoBodySignedOut", IDS_HISTORY_SYNC_PROMO_BODY_SIGNED_OUT},
      {"historySyncPromoBodySignInPending",
       IDS_HISTORY_SYNC_PROMO_BODY_SIGN_IN_PENDING},
      {"historySyncPromoBodySignInPendingSyncHistoryOn",
       IDS_HISTORY_SYNC_PROMO_BODY_SIGN_IN_PENDING_SYNC_HISTORY_ON},
  };
  source->AddLocalizedStrings(kHistorySyncStrings);
#endif  // BUILDFLAG(IS_CHROMEOS)

  bool enable_history_embeddings =
      history_embeddings::IsHistoryEmbeddingsEnabledForProfile(profile);
  source->AddBoolean("enableHistoryEmbeddings", enable_history_embeddings);
  source->AddBoolean(
      "maybeShowEmbeddingsIph",
      history_embeddings::IsHistoryEmbeddingsSettingVisible(profile) &&
          !enable_history_embeddings);

  static constexpr webui::LocalizedString kHistoryEmbeddingsStrings[] = {
      {"historyEmbeddingsPromoLabel", IDS_HISTORY_EMBEDDINGS_PROMO_LABEL},
      {"historyEmbeddingsPromoClose", IDS_HISTORY_EMBEDDINGS_PROMO_CLOSE},
      {"historyEmbeddingsPromoHeading", IDS_HISTORY_EMBEDDINGS_PROMO_HEADING},
      {"historyEmbeddingsPromoBody", IDS_HISTORY_EMBEDDINGS_PROMO_BODY},
      {"historyEmbeddingsAnswersPromoHeading",
       IDS_HISTORY_EMBEDDINGS_ANSWERS_PROMO_HEADING},
      {"historyEmbeddingsAnswersPromoBody",
       IDS_HISTORY_EMBEDDINGS_ANSWERS_PROMO_BODY},
      {"historyEmbeddingsPromoSettingsLinkText",
       IDS_HISTORY_EMBEDDIGNS_PROMO_SETTINGS_LINK_TEXT},
  };
  source->AddLocalizedStrings(kHistoryEmbeddingsStrings);

  // History clusters
  HistoryClustersUtil::PopulateSource(source, profile, /*in_side_panel=*/false);

  return source;
}

}  // namespace

HistoryUIConfig::HistoryUIConfig()
    : WebUIConfig(content::kChromeUIScheme, chrome::kChromeUIHistoryHost) {}

HistoryUIConfig::~HistoryUIConfig() = default;

std::unique_ptr<content::WebUIController>
HistoryUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                       const GURL& url) {
  Profile* profile = Profile::FromWebUI(web_ui);
  if (profile->IsGuestSession()) {
    return std::make_unique<PageNotAvailableForGuestUI>(
        web_ui, chrome::kChromeUIHistoryHost);
  }
  return std::make_unique<HistoryUI>(web_ui);
}

HistoryUI::HistoryUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* data_source =
      CreateAndAddHistoryUIHTMLSource(profile);
  ManagedUIHandler::Initialize(web_ui, data_source);

  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(history_clusters::prefs::kVisible,
                             base::BindRepeating(&HistoryUI::UpdateDataSource,
                                                 base::Unretained(this)));

  web_ui->AddMessageHandler(std::make_unique<webui::NavigationHandler>());
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());

  auto foreign_session_handler =
      std::make_unique<browser_sync::ForeignSessionHandler>();
  browser_sync::ForeignSessionHandler* foreign_session_handler_ptr =
      foreign_session_handler.get();
  web_ui->AddMessageHandler(std::move(foreign_session_handler));
  foreign_session_handler_ptr->InitializeForeignSessions();
  web_ui->AddMessageHandler(
      std::make_unique<HistoryLoginHandler>(base::BindRepeating(
          &HistoryUI::UpdateDataSource, base::Unretained(this))));
}

HistoryUI::~HistoryUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(HistoryUI)

// static
base::RefCountedMemory* HistoryUI::GetFaviconResourceBytes(
    ui::ResourceScaleFactor scale_factor) {
  return static_cast<base::RefCountedMemory*>(
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
          IDR_HISTORY_FAVICON, scale_factor));
}

void HistoryUI::BindInterface(
    mojo::PendingReceiver<history_embeddings::mojom::PageHandler>
        pending_page_handler) {
  history_embeddings_handler_ = std::make_unique<HistoryEmbeddingsHandler>(
      std::move(pending_page_handler),
      Profile::FromWebUI(web_ui())->GetWeakPtr(), web_ui(),
      /*for_side_panel=*/false);
}

void HistoryUI::BindInterface(
    mojo::PendingReceiver<history::mojom::PageHandler> pending_page_handler) {
  browsing_history_handler_ = std::make_unique<BrowsingHistoryHandler>(
      std::move(pending_page_handler), Profile::FromWebUI(web_ui()),
      web_ui()->GetWebContents());
}

void HistoryUI::BindInterface(
    mojo::PendingReceiver<history_clusters::mojom::PageHandler>
        pending_page_handler) {
  history_clusters_handler_ =
      std::make_unique<history_clusters::HistoryClustersHandler>(
          std::move(pending_page_handler), Profile::FromWebUI(web_ui()),
          web_ui()->GetWebContents(),
          // HistoryUI should always be in a tab. Look it up unconditionally.
          tabs::TabInterface::GetFromContents(web_ui()->GetWebContents()));
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

  Profile* profile = Profile::FromWebUI(web_ui());

  base::Value::Dict update;

  const bool is_managed = profile->GetPrefs()->IsManagedPreference(
      history_clusters::prefs::kVisible);
  // History clusters are always visible unless the visibility prefs
  // is set to false by policy.
  update.Set(
      kIsHistoryClustersVisibleKey,
      profile->GetPrefs()->GetBoolean(history_clusters::prefs::kVisible) ||
          !is_managed);
  update.Set(kIsHistoryClustersVisibleManagedByPolicyKey, is_managed);

  content::WebUIDataSource::Update(profile, chrome::kChromeUIHistoryHost,
                                   std::move(update));
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
      std::move(handler), std::move(client), this,
      std::vector<ui::ElementIdentifier>{kHistorySearchInputElementId});
}
