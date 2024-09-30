// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

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
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/history_embeddings/history_embeddings_utils.h"
#include "chrome/browser/page_image_service/image_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/commerce/shopping_ui_handler_delegate.h"
#include "chrome/browser/ui/webui/cr_components/history_clusters/history_clusters_util.h"
#include "chrome/browser/ui/webui/cr_components/history_embeddings/history_embeddings_handler.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/history/browsing_history_handler.h"
#include "chrome/browser/ui/webui/history/foreign_session_handler.h"
#include "chrome/browser/ui/webui/history/history_login_handler.h"
#include "chrome/browser/ui/webui/history/navigation_handler.h"
#include "chrome/browser/ui/webui/history_clusters/history_clusters_handler.h"
#include "chrome/browser/ui/webui/managed_ui_handler.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/page_not_available_for_guest/page_not_available_for_guest_ui.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/history_resources.h"
#include "chrome/grit/history_resources_map.h"
#include "chrome/grit/locale_settings.h"
#include "components/commerce/core/feature_utils.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/webui/shopping_service_handler.h"
#include "components/favicon_base/favicon_url_parser.h"
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
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/webui/resources/cr_components/commerce/shopping_service.mojom.h"

namespace {

constexpr char kIsUserSignedInKey[] = "isUserSignedIn";

bool IsUserSignedIn(Profile* profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  return identity_manager &&
         identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync);
}

content::WebUIDataSource* CreateAndAddHistoryUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIHistoryHost);

  static constexpr webui::LocalizedString kStrings[] = {
      // Localized strings (alphabetical order).
      {"actionMenuDescription", IDS_HISTORY_ACTION_MENU_DESCRIPTION},
      {"ariaRoleDescription", IDS_HISTORY_ARIA_ROLE_DESCRIPTION},
      {"bookmarked", IDS_HISTORY_ENTRY_BOOKMARKED},
      {"cancel", IDS_CANCEL},
      {"clearBrowsingData", IDS_CLEAR_BROWSING_DATA_TITLE},
      {"clearBrowsingDataLinkTooltip", IDS_SETTINGS_OPENS_IN_NEW_TAB},
      {"clearSearch", IDS_CLEAR_SEARCH},
      {"collapseSessionButton", IDS_HISTORY_OTHER_SESSIONS_COLLAPSE_SESSION},
      {"compareHistoryEmpty", IDS_COMPARE_HISTORY_EMPTY},
      {"compareHistoryRemove", IDS_COMPARE_HISTORY_REMOVE},
      {"compareHistoryHeader", IDS_COMPARE_HISTORY_HEADER},
      {"compareHistoryInfo", IDS_COMPARE_HISTORY_INFO},
      {"compareHistoryListsMenuItem", IDS_COMPARE_HISTORY_MENU_ITEM},
      {"compareHistoryRow", IDS_COMPARE_HISTORY_ROW},
      {"compareHistoryMenuAriaLabel", IDS_COMPARE_HISTORY_MENU_ARIA_LABEL},
      {"delete", IDS_HISTORY_DELETE},
      {"deleteSuccess", IDS_HISTORY_REMOVE_PAGE_SUCCESS},
      {"deleteConfirm", IDS_HISTORY_DELETE_PRIOR_VISITS_CONFIRM_BUTTON},
      {"deleteSession", IDS_HISTORY_OTHER_SESSIONS_HIDE_FOR_NOW},
      {"deleteWarning", IDS_HISTORY_DELETE_PRIOR_VISITS_WARNING},
      {"entrySummary", IDS_HISTORY_ENTRY_SUMMARY},
      {"expandSessionButton", IDS_HISTORY_OTHER_SESSIONS_EXPAND_SESSION},
      {"foundSearchResults", IDS_HISTORY_FOUND_SEARCH_RESULTS},
      {"historyMenuButton", IDS_HISTORY_HISTORY_MENU_DESCRIPTION},
      {"historyMenuItem", IDS_HISTORY_HISTORY_MENU_ITEM},
      {"itemsSelected", IDS_HISTORY_ITEMS_SELECTED},
      {"itemsUnselected", IDS_HISTORY_ITEMS_UNSELECTED},
      {"loading", IDS_HISTORY_LOADING},
      {"menu", IDS_MENU},
      {"moreFromSite", IDS_HISTORY_MORE_FROM_SITE},
      {"openAll", IDS_HISTORY_OTHER_SESSIONS_OPEN_ALL},
      {"openTabsMenuItem", IDS_HISTORY_OPEN_TABS_MENU_ITEM},
      {"noResults", IDS_HISTORY_NO_RESULTS},
      {"noSearchResults", IDS_HISTORY_NO_SEARCH_RESULTS},
      {"noSyncedResults", IDS_HISTORY_NO_SYNCED_RESULTS},
      {"removeBookmark", IDS_HISTORY_REMOVE_BOOKMARK},
      {"removeFromHistory", IDS_HISTORY_REMOVE_PAGE},
      {"removeSelected", IDS_HISTORY_REMOVE_SELECTED_ITEMS},
      {"searchPrompt", IDS_HISTORY_SEARCH_PROMPT},
      {"searchResult", IDS_HISTORY_SEARCH_RESULT},
      {"searchResults", IDS_HISTORY_SEARCH_RESULTS},
      {"turnOnSyncPromo", IDS_HISTORY_TURN_ON_SYNC_PROMO},
      {"turnOnSyncPromoDesc", IDS_HISTORY_TURN_ON_SYNC_PROMO_DESC},
      {"title", IDS_HISTORY_TITLE},
      {"compareHistorySyncMessage", IDS_COMPARE_SYNC_PROMO_MESSAGE},
      {"compareHistorySyncDescription", IDS_COMPARE_SYNC_PROMO_DESCRIPTION},
      {"compareHistorySyncButton", IDS_COMPARE_SYNC_PROMO_BUTTON},
      {"compareHistoryErrorMessage", IDS_COMPARE_ERROR_TITLE},
      {"compareHistoryErrorDescription", IDS_COMPARE_ERROR_DESCRIPTION},
  };
  source->AddLocalizedStrings(kStrings);

  source->AddString(
      "sidebarFooter",
      l10n_util::GetStringFUTF16(
          IDS_HISTORY_OTHER_FORMS_OF_HISTORY,
          l10n_util::GetStringUTF16(
              IDS_SETTINGS_CLEAR_DATA_MYACTIVITY_URL_IN_HISTORY)));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  source->AddLocalizedString("turnOnSyncButton",
                             IDS_HISTORY_TURN_ON_SYNC_BUTTON);
#else
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  bool has_primary_account =
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin);
  AccountInfo account_info =
      signin_ui_util::GetSingleAccountForPromos(identity_manager);
  if (switches::IsExplicitBrowserSigninUIOnDesktopEnabled() &&
      !has_primary_account && !account_info.IsEmpty()) {
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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  PrefService* prefs = profile->GetPrefs();
  bool allow_deleting_history =
      prefs->GetBoolean(prefs::kAllowDeletingBrowserHistory);
  source->AddBoolean("allowDeletingHistory", allow_deleting_history);

  source->AddBoolean("isGuestSession", profile->IsGuestSession());
  source->AddBoolean("isSignInAllowed",
                     prefs->GetBoolean(prefs::kSigninAllowed));

  source->AddBoolean(kIsUserSignedInKey, IsUserSignedIn(profile));

  source->AddInteger(
      "lastSelectedTab",
      prefs->GetInteger(history_clusters::prefs::kLastSelectedTab));

  bool enable_history_embeddings =
      history_embeddings::IsHistoryEmbeddingsEnabledForProfile(profile);
  source->AddBoolean("enableHistoryEmbeddings", enable_history_embeddings);
  source->AddBoolean(
      "maybeShowEmbeddingsIph",
      history_embeddings::IsHistoryEmbeddingsSettingVisible(profile) &&
          !enable_history_embeddings);
  history_embeddings::PopulateSourceForWebUI(source);

  // History clusters
  HistoryClustersUtil::PopulateSource(source, profile, /*in_side_panel=*/false);

  webui::SetupWebUIDataSource(
      source, base::make_span(kHistoryResources, kHistoryResourcesSize),
      IDR_HISTORY_HISTORY_HTML);

  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));

  // Product specifications:
  commerce::ShoppingService* service =
      commerce::ShoppingServiceFactory::GetForBrowserContext(profile);
  // Used to determine when the compare tab on history sidepanel is shown.
  source->AddBoolean("compareHistoryEnabled",
                     commerce::CanLoadProductSpecificationsFullPageUi(
                         service->GetAccountChecker()));
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
  auto browsing_history_handler = std::make_unique<BrowsingHistoryHandler>();
  BrowsingHistoryHandler* browsing_history_handler_ptr =
      browsing_history_handler.get();
  web_ui->AddMessageHandler(std::move(browsing_history_handler));
  browsing_history_handler_ptr->StartQueryHistory();
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
      Profile::FromWebUI(web_ui())->GetWeakPtr(), web_ui());
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

void HistoryUI::BindInterface(
    mojo::PendingReceiver<
        shopping_service::mojom::ShoppingServiceHandlerFactory> receiver) {
  shopping_service_factory_receiver_.reset();
  shopping_service_factory_receiver_.Bind(std::move(receiver));
}

void HistoryUI::CreateShoppingServiceHandler(
    mojo::PendingRemote<shopping_service::mojom::Page> page,
    mojo::PendingReceiver<shopping_service::mojom::ShoppingServiceHandler>
        receiver) {
  Profile* const profile = Profile::FromWebUI(web_ui());
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  commerce::ShoppingService* shopping_service =
      commerce::ShoppingServiceFactory::GetForBrowserContext(profile);
  feature_engagement::Tracker* const tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(profile);
  shopping_service_handler_ =
      std::make_unique<commerce::ShoppingServiceHandler>(
          std::move(page), std::move(receiver), bookmark_model,
          shopping_service, profile->GetPrefs(), tracker,
          std::make_unique<commerce::ShoppingUiHandlerDelegate>(nullptr,
                                                                profile),
          nullptr);
}

void HistoryUI::UpdateDataSource() {
  CHECK(web_ui());

  Profile* profile = Profile::FromWebUI(web_ui());

  base::Value::Dict update;
  update.Set(kIsUserSignedInKey, IsUserSignedIn(profile));

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
