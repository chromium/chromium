// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_search/tab_search_ui.h"

#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "build/branding_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service_factory.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter_service.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_page_handler.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_prefs.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_sync_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/tab_search_resources.h"
#include "chrome/grit/tab_search_resources_map.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/views/style/platform_style.h"
#include "ui/webui/webui_util.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#endif

TabSearchUIConfig::TabSearchUIConfig()
    : DefaultTopChromeWebUIConfig(content::kChromeUIScheme,
                                  chrome::kChromeUITabSearchHost) {}

bool TabSearchUIConfig::ShouldAutoResizeHost() {
  return true;
}

bool TabSearchUIConfig::IsPreloadable() {
  return true;
}

std::optional<int> TabSearchUIConfig::GetCommandIdForTesting() {
  return IDC_TAB_SEARCH;
}

TabSearchUI::TabSearchUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui,
                               true /* Needed for webui browser tests */),
      webui_load_timer_(web_ui->GetWebContents(),
                        "Tabs.TabSearch.WebUI.LoadDocumentTime",
                        "Tabs.TabSearch.WebUI.LoadCompletedTime") {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUITabSearchHost);
  static constexpr webui::LocalizedString kStrings[] = {
      // Tab search UI strings
      {"a11yTabClosed", IDS_TAB_SEARCH_A11Y_TAB_CLOSED},
      {"a11yFoundTab", IDS_TAB_SEARCH_A11Y_FOUND_TAB},
      {"a11yFoundTabFor", IDS_TAB_SEARCH_A11Y_FOUND_TAB_FOR},
      {"a11yFoundTabs", IDS_TAB_SEARCH_A11Y_FOUND_TABS},
      {"a11yFoundTabsFor", IDS_TAB_SEARCH_A11Y_FOUND_TABS_FOR},
      {"a11yOpenTab", IDS_TAB_SEARCH_A11Y_OPEN_TAB},
      {"a11yRecentlyClosedTab", IDS_TAB_SEARCH_A11Y_RECENTLY_CLOSED_TAB},
      {"a11yRecentlyClosedTabGroup",
       IDS_TAB_SEARCH_A11Y_RECENTLY_CLOSED_TAB_GROUP},
      {"audioMuting", IDS_TAB_AX_LABEL_AUDIO_MUTING_FORMAT},
      {"audioPlaying", IDS_TAB_AX_LABEL_AUDIO_PLAYING_FORMAT},
      {"blobUrlSource", IDS_HOVER_CARD_BLOB_URL_SOURCE},
      {"clearSearch", IDS_CLEAR_SEARCH},
      {"closeTab", IDS_TAB_SEARCH_CLOSE_TAB},
      {"collapseRecentlyClosed", IDS_TAB_SEARCH_COLLAPSE_RECENTLY_CLOSED},
      {"expandRecentlyClosed", IDS_TAB_SEARCH_EXPAND_RECENTLY_CLOSED},
      {"fileUrlSource", IDS_HOVER_CARD_FILE_URL_SOURCE},
      {"mediaRecording", IDS_TAB_AX_LABEL_MEDIA_RECORDING_FORMAT},
      {"audioRecording", IDS_TAB_AX_LABEL_AUDIO_RECORDING_FORMAT},
      {"videoRecording", IDS_TAB_AX_LABEL_VIDEO_RECORDING_FORMAT},
      {"mediaTabs", IDS_TAB_SEARCH_MEDIA_TABS},
      {"noResultsFound", IDS_TAB_SEARCH_NO_RESULTS_FOUND},
      {"openTabs", IDS_TAB_SEARCH_OPEN_TABS},
      {"oneTab", IDS_TAB_SEARCH_ONE_TAB},
      {"recentlyClosed", IDS_TAB_SEARCH_RECENTLY_CLOSED},
      {"recentlyClosedExpandA11yLabel",
       IDS_TAB_SEARCH_EXPAND_RECENTLY_CLOSED_ITEMS},
      {"searchTabs", IDS_TAB_SEARCH_SEARCH_TABS},
      {"tabCount", IDS_TAB_SEARCH_TAB_COUNT},
      {"tabSearchTabName", IDS_TAB_SEARCH_TAB_NAME},
      // Auto tab groups UI strings
      {"a11yTabExcludedFromGroup", IDS_TAB_ORGANIZATION_A11Y_TAB_EXCLUDED},
      {"clearAriaLabel", IDS_TAB_ORGANIZATION_CLEAR_ARIA_LABEL},
      {"clearSuggestions", IDS_TAB_ORGANIZATION_CLEAR_SUGGESTIONS},
      {"createGroup", IDS_TAB_ORGANIZATION_CREATE_GROUP},
      {"createGroups", IDS_TAB_ORGANIZATION_CREATE_GROUPS},
      {"dismiss", IDS_TAB_ORGANIZATION_DISMISS},
      {"editAriaLabel", IDS_TAB_ORGANIZATION_EDIT_ARIA_LABEL},
      {"failureBodyGeneric", IDS_TAB_ORGANIZATION_FAILURE_BODY_GENERIC},
      {"failureBodyGrouping", IDS_TAB_ORGANIZATION_FAILURE_BODY_GROUPING},
      {"failureTitleGeneric", IDS_TAB_ORGANIZATION_FAILURE_TITLE_GENERIC},
      {"failureTitleGrouping", IDS_TAB_ORGANIZATION_FAILURE_TITLE_GROUPING},
      {"inProgressTitle", IDS_TAB_ORGANIZATION_IN_PROGRESS_TITLE},
      {"inputAriaLabel", IDS_TAB_ORGANIZATION_INPUT_ARIA_LABEL},
      {"learnMore", IDS_TAB_ORGANIZATION_LEARN_MORE},
      {"learnMoreAriaLabel", IDS_TAB_ORGANIZATION_LEARN_MORE_ARIA_LABEL},
      {"learnMoreDisclaimer1", IDS_TAB_ORGANIZATION_DISCLAIMER_1},
      {"learnMoreDisclaimer2", IDS_TAB_ORGANIZATION_DISCLAIMER_2},
      {"newTabs", IDS_TAB_ORGANIZATION_NEW_TABS},
      {"notStartedBody", IDS_TAB_ORGANIZATION_NOT_STARTED_BODY},
      {"notStartedBodyFREHeader",
       IDS_TAB_ORGANIZATION_NOT_STARTED_BODY_FRE_HEADER},
      {"notStartedBodyFREBullet1",
       IDS_TAB_ORGANIZATION_NOT_STARTED_BODY_FRE_BULLET_1},
      {"notStartedBodyFREBullet2",
       IDS_TAB_ORGANIZATION_NOT_STARTED_BODY_FRE_BULLET_2},
      {"notStartedBodyFREBullet3",
       IDS_TAB_ORGANIZATION_NOT_STARTED_BODY_FRE_BULLET_3},
      {"notStartedBodySignedOut",
       IDS_TAB_ORGANIZATION_NOT_STARTED_BODY_SIGNED_OUT},
      {"notStartedButton", IDS_TAB_ORGANIZATION_NOT_STARTED_BUTTON},
      {"notStartedButtonAriaLabel",
       IDS_TAB_ORGANIZATION_NOT_STARTED_BUTTON_ARIA_LABEL},
      {"notStartedButtonFRE", IDS_TAB_ORGANIZATION_NOT_STARTED_BUTTON_FRE},
      {"notStartedButtonFREAriaLabel",
       IDS_TAB_ORGANIZATION_NOT_STARTED_BUTTON_FRE_ARIA_LABEL},
      {"notStartedButtonSignedOut",
       IDS_TAB_ORGANIZATION_NOT_STARTED_BUTTON_SIGNED_OUT},
      {"notStartedButtonSignedOutAriaLabel",
       IDS_TAB_ORGANIZATION_NOT_STARTED_BUTTON_SIGNED_OUT_ARIA_LABEL},
      {"notStartedTitle", IDS_TAB_ORGANIZATION_NOT_STARTED_TITLE},
      {"notStartedTitleFRE", IDS_TAB_ORGANIZATION_NOT_STARTED_TITLE_FRE},
      {"rejectAriaLabel", IDS_TAB_ORGANIZATION_REJECT_ARIA_LABEL},
      {"successMissingActiveTabTitle",
       IDS_TAB_ORGANIZATION_SUCCESS_MISSING_ACTIVE_TAB_TITLE},
      {"successTitleSingle", IDS_TAB_ORGANIZATION_SUCCESS_TITLE_SINGLE},
      {"successTitleMulti", IDS_TAB_ORGANIZATION_SUCCESS_TITLE_MULTI},
      {"tabOrganizationCloseTabAriaLabel",
       IDS_TAB_ORGANIZATION_CLOSE_TAB_ARIA_LABEL},
      {"tabOrganizationCloseTabTooltip",
       IDS_TAB_ORGANIZATION_CLOSE_TAB_TOOLTIP},
      {"tabOrganizationTabName", IDS_TAB_ORGANIZATION_TAB_NAME},
      {"tipAction", IDS_TAB_ORGANIZATION_TIP_ACTION},
      {"tipAriaDescription", IDS_TAB_ORGANIZATION_TIP_ARIA_DESCRIPTION},
      {"tipBody", IDS_TAB_ORGANIZATION_TIP_BODY},
      {"tipTitle", IDS_TAB_ORGANIZATION_TIP_TITLE},
      {"thumbsDown", IDS_TAB_ORGANIZATION_THUMBS_DOWN},
      {"thumbsUp", IDS_TAB_ORGANIZATION_THUMBS_UP},
      // Declutter UI strings
      {"a11yTabExcludedFromList", IDS_DECLUTTER_A11Y_TAB_EXCLUDED},
      {"closeTabs", IDS_DECLUTTER_CLOSE_TABS},
      {"declutterCloseTabAriaLabel", IDS_DECLUTTER_CLOSE_TAB_ARIA_LABEL},
      {"declutterCloseTabTooltip", IDS_DECLUTTER_CLOSE_TAB_TOOLTIP},
      {"declutterDuplicateBody", IDS_DECLUTTER_DUPLICATE_BODY},
      {"declutterDuplicateTitle", IDS_DECLUTTER_DUPLICATE_TITLE},
      {"declutterEmptyBody", IDS_DECLUTTER_EMPTY_BODY},
      {"declutterEmptyBodyNoDedupe", IDS_DECLUTTER_EMPTY_BODY_NO_DEDUPE},
      {"declutterEmptyTitle", IDS_DECLUTTER_EMPTY_TITLE},
      {"declutterInactiveTitle", IDS_DECLUTTER_INACTIVE_TITLE},
      {"declutterInactiveTitleNoDedupe",
       IDS_DECLUTTER_INACTIVE_TITLE_NO_DEDUPE},
      {"declutterTimestamp", IDS_DECLUTTER_TIMESTAMP},
      {"declutterTitle", IDS_DECLUTTER_TITLE},
      {"duplicateItemTitleMulti", IDS_DUPLICATE_ITEM_TITLE_MULTI},
      {"duplicateItemTitleSingle", IDS_DUPLICATE_ITEM_TITLE_SINGLE},
      // Selector UI strings
      {"autoTabGroupsSelectorHeading", IDS_AUTO_TAB_GROUPS_SELECTOR_HEADING},
      {"autoTabGroupsSelectorSubheading",
       IDS_AUTO_TAB_GROUPS_SELECTOR_SUBHEADING},
      {"backButtonAriaLabel", IDS_TAB_ORGANIZATION_BACK_BUTTON_ARIA_LABEL},
      {"declutterSelectorSubheading", IDS_DECLUTTER_SELECTOR_SUBHEADING},
      {"selectorAriaLabel", IDS_TAB_ORGANIZATION_SELECTOR_ARIA_LABEL},
      // Split view new tab page strings
      {"splitViewEmptyBody", IDS_SPLIT_VIEW_NTP_EMPTY_BODY},
      {"splitViewEmptyTitle", IDS_SPLIT_VIEW_NTP_EMPTY_TITLE},
      {"splitViewTabTitle", IDS_SPLIT_VIEW_NTP_TAB_TITLE},
      {"splitViewTitle", IDS_SPLIT_VIEW_NTP_TITLE},
      {"splitViewCloseButtonAriaLabel",
       IDS_SPLIT_VIEW_NTP_CLOSE_BUTTON_ARIA_LABEL},
  };
  source->AddLocalizedStrings(kStrings);
  source->AddBoolean("useRipples", views::PlatformStyle::kUseRipples);

  source->AddLocalizedString("close", IDS_CLOSE);

  source->AddInteger("recentlyClosedDefaultItemDisplayCount",
                     TabSearchPageHandler::kMinRecentlyClosedItemDisplayCount);

  bool tab_organization_enabled = false;
  if (TabOrganizationUtils::GetInstance()->IsEnabled(profile)) {
    const auto* const tab_organization_service =
        TabOrganizationServiceFactory::GetForProfile(profile);
    if (tab_organization_service) {
      tab_organization_enabled = true;
    }
  }
  source->AddBoolean("tabOrganizationEnabled", tab_organization_enabled);
  source->AddBoolean(
      "tabOrganizationModelStrategyEnabled",
      base::FeatureList::IsEnabled(features::kTabOrganizationModelStrategy));
  source->AddBoolean(
      "TabOrganizationUserInstructionEnabled",
      base::FeatureList::IsEnabled(features::kTabOrganizationUserInstruction));

  source->AddBoolean("showTabOrganizationFRE", ShowTabOrganizationFRE());
  source->AddBoolean(
      "declutterEnabled",
      features::IsTabstripDeclutterEnabled() && !profile->IsIncognitoProfile());
  source->AddBoolean("dedupeEnabled", features::IsTabstripDedupeEnabled() &&
                                          !profile->IsIncognitoProfile());
  source->AddBoolean("splitViewEnabled",
                     base::FeatureList::IsEnabled(features::kSideBySide));

#if BUILDFLAG(ENABLE_GLIC)
  source->AddResourcePath("alert_indicators/tab_media_glic_active.svg",
                          IDR_GLIC_TAB_MEDIA_GLIC_ACTIVE);
#endif

  ui::Accelerator accelerator(ui::VKEY_A,
                              ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR);
  source->AddString("shortcutText", accelerator.GetShortcutText());
  // TODO(b/362269642): Once the stale threshold duration is Finch-
  // configurable, replace the hardcoded 7 below with the value of that
  // parameter.
  source->AddString(
      "declutterInactiveBody",
      l10n_util::GetStringFUTF16(IDS_DECLUTTER_INACTIVE_BODY, u"7"));
  source->AddString("newTabPageUrl", chrome::kChromeUINewTabPageURL);

  webui::SetupWebUIDataSource(source, kTabSearchResources,
                              IDR_TAB_SEARCH_TAB_SEARCH_HTML);

  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));

  auto plural_string_handler = std::make_unique<PluralStringHandler>();
  plural_string_handler->AddLocalizedString(
      "declutterSelectorHeadingNoDedupe",
      IDS_DECLUTTER_SELECTOR_HEADING_NO_DEDUPE);
  plural_string_handler->AddLocalizedString("declutterSelectorHeading",
                                            IDS_DECLUTTER_SELECTOR_HEADING);
  web_ui->AddMessageHandler(std::move(plural_string_handler));

  web_ui->AddMessageHandler(std::make_unique<TabSearchSyncHandler>(profile));

  page_handler_timer_ = base::ElapsedTimer();
  TRACE_EVENT_BEGIN("browser", "TabSearchPageHandlerConstructionDelay",
                    perfetto::Track::FromPointer(this));
}

TabSearchUI::~TabSearchUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(TabSearchUI)

void TabSearchUI::BindInterface(
    mojo::PendingReceiver<tab_search::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void TabSearchUI::CreatePageHandler(
    mojo::PendingRemote<tab_search::mojom::Page> page,
    mojo::PendingReceiver<tab_search::mojom::PageHandler> receiver) {
  DCHECK(page);

  // CreatePageHandler() can be called multiple times if reusing the same
  // WebUIController. For eg refreshing the page will create new PageHandler but
  // reuse TabSearchUI. Check to make sure |page_handler_timer_| is valid before
  // logging metrics.
  if (page_handler_timer_.has_value()) {
    TRACE_EVENT_END("browser", perfetto::Track::FromPointer(this));
    UmaHistogramMediumTimes("Tabs.TabSearch.PageHandlerConstructionDelay",
                            page_handler_timer_->Elapsed());
    page_handler_timer_.reset();
  }

  MetricsReporterService* service =
      MetricsReporterService::GetFromWebContents(web_ui()->GetWebContents());

  // TODO(tluk): Investigate whether we can avoid recreating this multiple times
  // per instance of the TabSearchUI.
  page_handler_ = std::make_unique<TabSearchPageHandler>(
      std::move(receiver), std::move(page), web_ui(), this,
      service->metrics_reporter());

  if (!page_handler_creation_callback_.is_null()) {
    std::move(page_handler_creation_callback_).Run();
  }
}

bool TabSearchUI::ShowTabOrganizationFRE() {
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  return prefs->GetBoolean(tab_search_prefs::kTabOrganizationShowFRE);
}
