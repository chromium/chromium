// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_helpers.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/time/default_tick_clock.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/client_hints/client_hints_observer.h"
#include "chrome/browser/content_settings/mixed_content_settings_tab_helper.h"
#include "chrome/browser/content_settings/sound_content_setting_observer.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/data_use_measurement/data_use_web_contents_observer.h"
#include "chrome/browser/engagement/site_engagement_helper.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/external_protocol/external_protocol_observer.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/installable/installable_manager.h"
#include "chrome/browser/language/chrome_language_detection_tab_helper.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_observer.h"
#include "chrome/browser/metrics/oom/out_of_memory_reporter.h"
#include "chrome/browser/metrics/renderer_uptime_web_contents_observer.h"
#include "chrome/browser/net/net_error_tab_helper.h"
#include "chrome/browser/ntp_snippets/bookmark_last_visit_updater.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_initialize.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/plugins/pdf_plugin_placeholder_observer.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/predictors/loading_predictor_tab_helper.h"
#include "chrome/browser/prerender/prerender_tab_helper.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "chrome/browser/previews/resource_loading_hints/resource_loading_hints_web_contents_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/browser/safe_browsing/trigger_creator.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ssl/connection_help_tab_helper.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/subresource_filter/chrome_subresource_filter_client.h"
#include "chrome/browser/sync/sessions/sync_sessions_router_tab_helper.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router_factory.h"
#include "chrome/browser/tab_contents/navigation_metrics_recorder.h"
#include "chrome/browser/tracing/navigation_tracing.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/bloated_renderer/bloated_renderer_tab_helper.h"
#include "chrome/browser/ui/blocked_content/popup_blocker_tab_helper.h"
#include "chrome/browser/ui/blocked_content/popup_opener_tab_helper.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/javascript_dialogs/javascript_dialog_tab_helper.h"
#include "chrome/browser/ui/navigation_correction_tab_observer.h"
#include "chrome/browser/ui/omnibox/lookalike_url_navigation_observer.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/pdf/chrome_pdf_web_contents_helper_client.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/dom_distiller/content/browser/web_contents_main_frame_observer.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/download/content/factory/navigation_monitor_factory.h"
#include "components/download/content/public/download_navigation_observer.h"
#include "components/history/content/browser/web_contents_top_sites_observer.h"
#include "components/history/core/browser/top_sites.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/tracing/common/tracing_switches.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/android/oom_intervention/oom_intervention_tab_helper.h"
#include "chrome/browser/android/search_permissions/search_geolocation_disclosure_tab_helper.h"
#include "chrome/browser/android/tasks/task_tab_helper.h"
#include "chrome/browser/android/webapps/single_tab_mode_tab_helper.h"
#include "chrome/browser/banners/app_banner_manager_android.h"
#include "chrome/browser/ui/android/context_menu_helper.h"
#include "chrome/browser/ui/android/view_android_helper.h"
#else
#include "chrome/browser/banners/app_banner_manager_desktop.h"
#include "chrome/browser/plugins/plugin_observer.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer.h"
#include "chrome/browser/safe_browsing/safe_browsing_tab_observer.h"
#include "chrome/browser/thumbnails/thumbnail_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/hung_plugin_tab_helper.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/sync/browser_synced_tab_delegate.h"
#include "components/pdf/browser/pdf_web_contents_helper.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/zoom/zoom_controller.h"
#endif  // defined(OS_ANDROID)

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
#include "chrome/browser/ui/blocked_content/framebust_block_tab_helper.h"
#include "chrome/browser/ui/hats/hats_helper.h"
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/offline_page_tab_helper.h"
#include "chrome/browser/offline_pages/recent_tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "chrome/browser/captive_portal/captive_portal_tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "extensions/browser/view_type_utils.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "chrome/browser/printing/printing_init.h"
#endif

using content::WebContents;

namespace {

const char kTabContentsAttachedTabHelpersUserDataKey[] =
    "TabContentsAttachedTabHelpers";

}  // namespace

// static
void TabHelpers::AttachTabHelpers(WebContents* web_contents) {
  // If already adopted, nothing to be done.
  base::SupportsUserData::Data* adoption_tag =
      web_contents->GetUserData(&kTabContentsAttachedTabHelpersUserDataKey);
  if (adoption_tag)
    return;

  // Mark as adopted.
  web_contents->SetUserData(&kTabContentsAttachedTabHelpersUserDataKey,
                            std::make_unique<base::SupportsUserData::Data>());

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Set the view type.
  extensions::SetViewType(web_contents, extensions::VIEW_TYPE_TAB_CONTENTS);
#endif

  // Create all the tab helpers.

  // SessionTabHelper comes first because it sets up the tab ID, and other
  // helpers may rely on that.
  SessionTabHelper::CreateForWebContents(web_contents);

#if !defined(OS_ANDROID)
  // ZoomController comes before common tab helpers since ChromeAutofillClient
  // may want to register as a ZoomObserver with it.
  zoom::ZoomController::CreateForWebContents(web_contents);
#endif

  // --- Common tab helpers ---
  autofill::ChromeAutofillClient::CreateForWebContents(web_contents);
  autofill::ContentAutofillDriverFactory::CreateForWebContentsAndDelegate(
      web_contents,
      autofill::ChromeAutofillClient::FromWebContents(web_contents),
      g_browser_process->GetApplicationLocale(),
      autofill::AutofillManager::ENABLE_AUTOFILL_DOWNLOAD_MANAGER);
  BloatedRendererTabHelper::CreateForWebContents(web_contents);
  BookmarkLastVisitUpdater::MaybeCreateForWebContentsWithBookmarkModel(
      web_contents, BookmarkModelFactory::GetForBrowserContext(
                        web_contents->GetBrowserContext()));
  chrome_browser_net::NetErrorTabHelper::CreateForWebContents(web_contents);
  ChromeLanguageDetectionTabHelper::CreateForWebContents(web_contents);
  ChromePasswordManagerClient::CreateForWebContentsWithAutofillClient(
      web_contents,
      autofill::ChromeAutofillClient::FromWebContents(web_contents));
  if (base::FeatureList::IsEnabled(
          subresource_filter::kSafeBrowsingSubresourceFilter)) {
    ChromeSubresourceFilterClient::CreateForWebContents(web_contents);
  }
  ChromeTranslateClient::CreateForWebContents(web_contents);
  ClientHintsObserver::CreateForWebContents(web_contents);
  ConnectionHelpTabHelper::CreateForWebContents(web_contents);
  CoreTabHelper::CreateForWebContents(web_contents);
  data_use_measurement::DataUseWebContentsObserver::CreateForWebContents(
      web_contents);
  ExternalProtocolObserver::CreateForWebContents(web_contents);
  favicon::CreateContentFaviconDriverForWebContents(web_contents);
  FindTabHelper::CreateForWebContents(web_contents);

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  download::DownloadNavigationObserver::CreateForWebContents(
      web_contents,
      download::NavigationMonitorFactory::GetForBrowserContext(profile));
  history::WebContentsTopSitesObserver::CreateForWebContents(
      web_contents, TopSitesFactory::GetForProfile(profile).get());
  HistoryTabHelper::CreateForWebContents(web_contents);
  InfoBarService::CreateForWebContents(web_contents);
  InstallableManager::CreateForWebContents(web_contents);
  metrics::RendererUptimeWebContentsObserver::CreateForWebContents(
      web_contents);
  MixedContentSettingsTabHelper::CreateForWebContents(web_contents);
  NavigationCorrectionTabObserver::CreateForWebContents(web_contents);
  NavigationMetricsRecorder::CreateForWebContents(web_contents);
  OutOfMemoryReporter::CreateForWebContents(web_contents);
  chrome::InitializePageLoadMetricsForWebContents(web_contents);
  PDFPluginPlaceholderObserver::CreateForWebContents(web_contents);
  PermissionRequestManager::CreateForWebContents(web_contents);
  // The PopupBlockerTabHelper has an implicit dependency on
  // ChromeSubresourceFilterClient being available in its constructor.
  PopupBlockerTabHelper::CreateForWebContents(web_contents);
  PopupOpenerTabHelper::CreateForWebContents(
      web_contents, base::DefaultTickClock::GetInstance());
  PrefsTabHelper::CreateForWebContents(web_contents);
  prerender::PrerenderTabHelper::CreateForWebContents(web_contents);
  PreviewsUITabHelper::CreateForWebContents(web_contents);
  RecentlyAudibleHelper::CreateForWebContents(web_contents);
  ResourceLoadingHintsWebContentsObserver::CreateForWebContents(web_contents);
  safe_browsing::TriggerCreator::MaybeCreateTriggersForWebContents(
      profile, web_contents);
  SearchEngineTabHelper::CreateForWebContents(web_contents);
  SecurityStateTabHelper::CreateForWebContents(web_contents);
  if (SiteEngagementService::IsEnabled())
    SiteEngagementService::Helper::CreateForWebContents(web_contents);
  if (base::FeatureList::IsEnabled(features::kSoundContentSetting))
    SoundContentSettingObserver::CreateForWebContents(web_contents);
  sync_sessions::SyncSessionsRouterTabHelper::CreateForWebContents(
      web_contents,
      sync_sessions::SyncSessionsWebContentsRouterFactory::GetForProfile(
          profile));
  // TODO(vabr): Remove TabSpecificContentSettings from here once their function
  // is taken over by ChromeContentSettingsClient. http://crbug.com/387075
  TabSpecificContentSettings::CreateForWebContents(web_contents);
  TabUIHelper::CreateForWebContents(web_contents);
  ukm::InitializeSourceUrlRecorderForWebContents(web_contents);
  vr::VrTabHelper::CreateForWebContents(web_contents);

  // NO! Do not just add your tab helper here. This is a large alphabetized
  // block; please insert your tab helper above in alphabetical order.

  // --- Platform-specific tab helpers ---

#if defined(OS_ANDROID)
  banners::AppBannerManagerAndroid::CreateForWebContents(web_contents);
  ContextMenuHelper::CreateForWebContents(web_contents);
  JavaScriptDialogTabHelper::CreateForWebContents(web_contents);
  if (OomInterventionTabHelper::IsEnabled()) {
    OomInterventionTabHelper::CreateForWebContents(web_contents);
  }

  SearchGeolocationDisclosureTabHelper::CreateForWebContents(web_contents);
  SingleTabModeTabHelper::CreateForWebContents(web_contents);
  tasks::TaskTabHelper::CreateForWebContents(web_contents);
  ViewAndroidHelper::CreateForWebContents(web_contents);
#else
  BookmarkTabHelper::CreateForWebContents(web_contents);
  BrowserSyncedTabDelegate::CreateForWebContents(web_contents);
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents);
  extensions::WebNavigationTabObserver::CreateForWebContents(web_contents);
  FramebustBlockTabHelper::CreateForWebContents(web_contents);
  HungPluginTabHelper::CreateForWebContents(web_contents);
  JavaScriptDialogTabHelper::CreateForWebContents(web_contents);
  if (base::FeatureList::IsEnabled(
          features::kLookalikeUrlNavigationSuggestions)) {
    LookalikeUrlNavigationObserver::CreateForWebContents(web_contents);
  }
  ManagePasswordsUIController::CreateForWebContents(web_contents);
  pdf::PDFWebContentsHelper::CreateForWebContentsWithClient(
      web_contents, std::unique_ptr<pdf::PDFWebContentsHelperClient>(
                        new ChromePDFWebContentsHelperClient()));
  PluginObserver::CreateForWebContents(web_contents);
  SadTabHelper::CreateForWebContents(web_contents);
  safe_browsing::SafeBrowsingTabObserver::CreateForWebContents(web_contents);
  safe_browsing::SafeBrowsingNavigationObserver::MaybeCreateForWebContents(
      web_contents);
  SearchTabHelper::CreateForWebContents(web_contents);
  TabDialogs::CreateForWebContents(web_contents);
  ThumbnailTabHelper::CreateForWebContents(web_contents);
  web_modal::WebContentsModalDialogManager::CreateForWebContents(web_contents);

  if (banners::AppBannerManagerDesktop::IsEnabled())
    banners::AppBannerManagerDesktop::CreateForWebContents(web_contents);
#endif

#if defined(OS_WIN) || defined(OS_MACOSX) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  metrics::DesktopSessionDurationObserver::CreateForWebContents(web_contents);
#endif

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
  if (base::FeatureList::IsEnabled(
          features::kHappinessTrackingSurveysForDesktop)) {
    HatsHelper::CreateForWebContents(web_contents);
  }
#endif

// --- Feature tab helpers behind flags ---

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
offline_pages::OfflinePageTabHelper::CreateForWebContents(web_contents);
offline_pages::RecentTabHelper::CreateForWebContents(web_contents);
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  CaptivePortalTabHelper::CreateForWebContents(web_contents);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::TabHelper::CreateForWebContents(web_contents);
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  SupervisedUserNavigationObserver::CreateForWebContents(web_contents);
#endif

#if BUILDFLAG(ENABLE_PRINTING) && !defined(OS_ANDROID)
  printing::InitializePrinting(web_contents);
#endif

  bool enabled_distiller = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableDomDistiller);
  if (enabled_distiller) {
    dom_distiller::WebContentsMainFrameObserver::CreateForWebContents(
        web_contents);
  }

  if (predictors::LoadingPredictorFactory::GetForProfile(profile))
    predictors::LoadingPredictorTabHelper::CreateForWebContents(web_contents);

  if (tracing::NavigationTracingObserver::IsEnabled())
    tracing::NavigationTracingObserver::CreateForWebContents(web_contents);

  if (MediaEngagementService::IsEnabled())
    MediaEngagementService::CreateWebContentsObserver(web_contents);

  resource_coordinator::ResourceCoordinatorTabHelper::CreateForWebContents(
      web_contents);
}
