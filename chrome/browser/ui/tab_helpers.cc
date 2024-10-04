// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_helpers.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/breadcrumbs/breadcrumb_manager_tab_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/captive_portal/captive_portal_service_factory.h"
#include "chrome/browser/chained_back_navigation_tracker.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/complex_tasks/task_tab_helper.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/mixed_content_settings_tab_helper.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/content_settings/sound_content_setting_observer.h"
#include "chrome/browser/dips/dips_bounce_detector.h"
#include "chrome/browser/dips/dips_navigation_flow_detector.h"
#include "chrome/browser/external_protocol/external_protocol_observer.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/file_system_access/file_system_access_features.h"
#include "chrome/browser/file_system_access/file_system_access_permission_request_manager.h"
#include "chrome/browser/file_system_access/file_system_access_tab_helper.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/history_clusters/history_clusters_tab_helper.h"
#include "chrome/browser/history_embeddings/history_embeddings_tab_helper.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/login_detection/login_detection_tab_helper.h"
#include "chrome/browser/lookalikes/safety_tip_web_contents_observer.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_observer.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_preconnect_client.h"
#include "chrome/browser/net/net_error_tab_helper.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_web_contents_observer.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_web_contents_observer.h"
#include "chrome/browser/page_info/about_this_site_tab_helper.h"
#include "chrome/browser/page_info/page_info_features.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_initialize.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_helper.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/predictors/loading_predictor_tab_helper.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/browser/safe_browsing/chrome_password_reuse_detection_manager_client.h"
#include "chrome/browser/safe_browsing/chrome_safe_browsing_tab_observer_delegate.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/tailored_security/tailored_security_service_factory.h"
#include "chrome/browser/safe_browsing/tailored_security/tailored_security_url_observer.h"
#include "chrome/browser/safe_browsing/trigger_creator.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/browser/site_protection/site_protection_metrics_observer.h"
#include "chrome/browser/ssl/chrome_security_blocking_page_factory.h"
#include "chrome/browser/ssl/chrome_security_state_tab_helper.h"
#include "chrome/browser/ssl/connection_help_tab_helper.h"
#include "chrome/browser/ssl/https_only_mode_tab_helper.h"
#include "chrome/browser/storage_access_api/storage_access_api_service_factory.h"
#include "chrome/browser/storage_access_api/storage_access_api_service_impl.h"
#include "chrome/browser/storage_access_api/storage_access_api_tab_helper.h"
#include "chrome/browser/subresource_filter/chrome_content_subresource_filter_web_contents_helper_factory.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#include "chrome/browser/sync/sessions/sync_sessions_router_tab_helper.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router_factory.h"
#include "chrome/browser/tab_contents/navigation_metrics_recorder.h"
#include "chrome/browser/tpcd/heuristics/opener_heuristic_tab_helper.h"
#include "chrome/browser/tpcd/heuristics/redirect_heuristic_tab_helper.h"
#include "chrome/browser/tpcd/http_error_observer/http_error_tab_helper.h"
#include "chrome/browser/tpcd/metadata/devtools_observer.h"
#include "chrome/browser/tpcd/support/validity_service.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/trusted_vault/trusted_vault_encryption_keys_tab_helper.h"
#include "chrome/browser/ui/autofill/autofill_client_provider.h"
#include "chrome/browser/ui/autofill/autofill_client_provider_factory.h"
#include "chrome/browser/ui/find_bar/find_bar_state.h"
#include "chrome/browser/ui/focus_tab_after_navigation_helper.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/performance_controls/memory_saver_chip_tab_helper.h"
#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/privacy_sandbox/privacy_sandbox_prompt_helper.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service_factory.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/thumbnails/thumbnail_tab_helper.h"
#include "chrome/browser/ui/views/side_panel/companion/companion_utils.h"
#include "chrome/browser/v8_compile_hints/v8_compile_hints_tab_helper.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/common/chrome_switches.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/blocked_content/popup_blocker_tab_helper.h"
#include "components/blocked_content/popup_opener_tab_helper.h"
#include "components/breadcrumbs/core/breadcrumbs_status.h"
#include "components/browsing_topics/browsing_topics_redirect_observer.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/client_hints/browser/client_hints_web_contents_observer.h"
#include "components/commerce/content/browser/commerce_tab_helper.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/compose/buildflags.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/download/content/factory/navigation_monitor_factory.h"
#include "components/download/content/public/download_navigation_observer.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/history/content/browser/web_contents_top_sites_observer.h"
#include "components/history/core/browser/top_sites.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager.h"
#include "components/metrics/content/metrics_services_web_contents_observer.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/page_info/core/features.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "components/performance_manager/public/features.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_recovery_success_rate_tracker.h"
#include "components/permissions/permission_request_manager.h"
#include "components/safe_browsing/content/browser/async_check_tracker.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer.h"
#include "components/safe_browsing/content/browser/safe_browsing_tab_observer.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/search/ntp_features.h"
#include "components/site_engagement/content/site_engagement_helper.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/tracing/common/tracing_switches.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/user_notes/user_notes_features.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "components/webapps/browser/installable/ml_installability_promoter.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "media/base/media_switches.h"
#include "net/base/features.h"
#include "pdf/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "rlz/buildflags/buildflags.h"
#include "third_party/blink/public/common/features.h"
#include "ui/accessibility/accessibility_features.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/android/oom_intervention/oom_intervention_tab_helper.h"
#include "chrome/browser/android/persisted_tab_data/sensitivity_persisted_tab_data_android.h"
#include "chrome/browser/android/policy/policy_auditor_bridge.h"
#include "chrome/browser/banners/android/chrome_app_banner_manager_android.h"
#include "chrome/browser/content_settings/request_desktop_site_web_contents_observer_android.h"
#include "chrome/browser/facilitated_payments/ui/chrome_facilitated_payments_client.h"
#include "chrome/browser/fast_checkout/fast_checkout_tab_helper.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/plugins/plugin_observer_android.h"
#include "chrome/browser/ui/android/context_menu_helper.h"
#include "chrome/browser/ui/javascript_dialogs/javascript_tab_modal_dialog_manager_delegate_android.h"
#include "components/facilitated_payments/core/features/features.h"
#include "components/sensitive_content/android/android_sensitive_content_client.h"
#include "components/sensitive_content/features.h"
#include "components/webapps/browser/android/app_banner_manager_android.h"
#include "content/public/common/content_features.h"
#else
#include "chrome/browser/banners/app_banner_manager_desktop.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_helper.h"
#include "chrome/browser/preloading/prefetch/zero_suggest_prefetch/zero_suggest_prefetch_tab_helper.h"
#include "chrome/browser/tab_contents/form_interaction_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/javascript_dialogs/javascript_tab_modal_dialog_manager_delegate_desktop.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/search_engine_choice/search_engine_choice_tab_helper.h"
#include "chrome/browser/ui/sync/browser_synced_tab_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/uma_browsing_activity_observer.h"
#include "chrome/browser/ui/views/side_panel/companion/companion_tab_helper.h"
#include "chrome/browser/ui/views/side_panel/companion/exps_registration_success_observer.h"
#include "chrome/browser/ui/views/side_panel/history_clusters/history_clusters_tab_helper.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_controller.h"
#include "components/commerce/content/browser/hint/commerce_hint_tab_helper.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/zoom/zoom_controller.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/side_search/side_search_tab_contents_helper.h"
#include "chrome/browser/ui/side_search/side_search_utils.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/boot_times_recorder/boot_times_recorder_tab_helper.h"
#include "chrome/browser/ash/growth/campaigns_manager_session_tab_helper.h"
#include "chrome/browser/ui/ash/google_one/google_one_offer_iph_tab_helper.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/web_contents_can_go_back_observer.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/chromeos/container_app/container_app_tab_helper.h"
#include "chrome/browser/chromeos/cros_apps/cros_apps_tab_helper.h"
#include "chrome/browser/chromeos/mahi/mahi_tab_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_tab_helper.h"
#include "chrome/browser/chromeos/printing/print_preview/printing_init_cros.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/blocked_content/framebust_block_tab_helper.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/hats/hats_helper.h"
#include "chrome/browser/ui/shared_highlighting/shared_highlighting_promo.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/font_prewarmer_tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "components/captive_portal/content/captive_portal_tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/extensions/navigation_extension_enabler.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/ui/extensions/extension_side_panel_utils.h"
#include "chrome/browser/ui/web_applications/web_app_metrics.h"
#include "chrome/browser/ui/web_applications/web_app_metrics_tab_helper.h"
#include "chrome/browser/web_applications/policy/pre_redirection_url_observer.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "extensions/browser/view_type_utils.h"  // nogncheck
#include "extensions/common/extension_features.h"
#include "extensions/common/mojom/view_type.mojom.h"
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/android/auto_fetch_page_load_watcher.h"
#include "chrome/browser/offline_pages/offline_page_tab_helper.h"
#include "chrome/browser/offline_pages/recent_tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/plugin_observer.h"
#include "chrome/browser/ui/hung_plugin_tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "chrome/browser/printing/printing_init.h"
#endif

#if BUILDFLAG(ENABLE_COMPOSE)
#include "chrome/browser/compose/chrome_compose_client.h"
#include "chrome/browser/compose/compose_enabling.h"
#include "components/compose/buildflags.h"
#include "components/compose/core/browser/compose_features.h"
#endif

#if BUILDFLAG(ENABLE_RLZ)
#include "chrome/browser/rlz/chrome_rlz_tracker_web_contents_observer.h"
#endif

#if BUILDFLAG(ENABLE_REPORTING)
#include "components/tpcd/enterprise_reporting/enterprise_reporting_tab_helper.h"
#endif

using content::WebContents;

namespace {

const char kTabContentsAttachedTabHelpersUserDataKey[] =
    "TabContentsAttachedTabHelpers";

}  // namespace

// static
// WARNING: Do not use this class for desktop chrome. Use TabFeatures instead.
// See
// https://chromium.googlesource.com/chromium/src/+/main/docs/chrome_browser_design_principles.md
void TabHelpers::AttachTabHelpers(WebContents* web_contents) {
  // If already adopted, nothing to be done.
  base::SupportsUserData::Data* adoption_tag =
      web_contents->GetUserData(&kTabContentsAttachedTabHelpersUserDataKey);
  if (adoption_tag) {
    return;
  }

  // Mark as adopted.
  web_contents->SetUserData(&kTabContentsAttachedTabHelpersUserDataKey,
                            std::make_unique<base::SupportsUserData::Data>());

  // Create all the tab helpers.

  // SessionTabHelper comes first because it sets up the tab ID, and other
  // helpers may rely on that.
  CreateSessionServiceTabHelper(web_contents);

#if !BUILDFLAG(IS_ANDROID)
  // ZoomController comes before common tab helpers since ChromeAutofillClient
  // may want to register as a ZoomObserver with it.
  zoom::ZoomController::CreateForWebContents(web_contents);
#endif

  // infobars::ContentInfoBarManager comes before common tab helpers since
  // ChromeSubresourceFilterClient has it as a dependency.
  infobars::ContentInfoBarManager::CreateForWebContents(web_contents);

  // `PageSpecificContentSettings` (PSCS) needs to come before
  // `DIPSWebContentsObserver` for this latter to be correctly added to the PSCS
  // observer list.
  content_settings::PageSpecificContentSettings::CreateForWebContents(
      web_contents,
      std::make_unique<PageSpecificContentSettingsDelegate>(web_contents));

  // RedirectChainDetector comes before common tab helpers since
  // DIPSWebContentsObserver has it as a dependency.
  RedirectChainDetector::CreateForWebContents(web_contents);

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  // --- Section 1: Common tab helpers ---
  if (page_info::IsAboutThisSiteAsyncFetchingEnabled()
#if defined(TOOLKIT_VIEWS)
      || page_info::IsPersistentSidePanelEntryFeatureEnabled()
#endif
  ) {
    if (auto* optimization_guide_decider =
            OptimizationGuideKeyedServiceFactory::GetForProfile(profile)) {
      AboutThisSiteTabHelper::CreateForWebContents(web_contents,
                                                   optimization_guide_decider);
    }
  }
  autofill::AutofillClientProvider& autofill_client_provider =
      autofill::AutofillClientProviderFactory::GetForProfile(profile);
  autofill_client_provider.CreateClientForWebContents(web_contents);
#if BUILDFLAG(IS_ANDROID)
  // The sensitive content client has to be instantiated after the autofill
  // client, because the sensitive content client starts a flow which uses
  // `ScopedAutofillManagersObservation`.
  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
          base::android::SdkVersion::SDK_VERSION_V &&
      base::FeatureList::IsEnabled(
          sensitive_content::features::kSensitiveContent)) {
    sensitive_content::AndroidSensitiveContentClient::CreateForWebContents(
        web_contents, "SensitiveContent.Chrome.");
  }
#endif  // BUILDFLAG(IS_ANDROID)
  if (breadcrumbs::IsEnabled(g_browser_process->local_state())) {
    BreadcrumbManagerTabHelper::CreateForWebContents(web_contents);
  }
  browsing_topics::BrowsingTopicsRedirectObserver::MaybeCreateForWebContents(
      web_contents);
  ChainedBackNavigationTracker::CreateForWebContents(web_contents);
  chrome_browser_net::NetErrorTabHelper::CreateForWebContents(web_contents);
  if (!autofill_client_provider.uses_platform_autofill()) {
    ChromePasswordManagerClient::CreateForWebContents(web_contents);
  }
  ChromePasswordReuseDetectionManagerClient::CreateForWebContents(web_contents);
  CreateSubresourceFilterWebContentsHelper(web_contents);
#if BUILDFLAG(ENABLE_RLZ)
  ChromeRLZTrackerWebContentsObserver::CreateForWebContentsIfNeeded(
      web_contents);
#endif
  ChromeSecurityStateTabHelper::CreateForWebContents(web_contents);
  ChromeTranslateClient::CreateForWebContents(web_contents);
  client_hints::ClientHintsWebContentsObserver::CreateForWebContents(
      web_contents);
  commerce::CommerceTabHelper::CreateForWebContents(
      web_contents, profile->IsOffTheRecord(),
      commerce::ShoppingServiceFactory::GetForBrowserContext(profile),
      ISOLATED_WORLD_ID_CHROME_INTERNAL);
  ConnectionHelpTabHelper::CreateForWebContents(web_contents);
  CoreTabHelper::CreateForWebContents(web_contents);
  DipsNavigationFlowDetector::CreateForWebContents(web_contents);
  DIPSWebContentsObserver::MaybeCreateForWebContents(web_contents);
#if BUILDFLAG(ENABLE_REPORTING)
  if (base::FeatureList::IsEnabled(
          net::features::kReportingApiEnableEnterpriseCookieIssues)) {
    tpcd::enterprise_reporting::EnterpriseReportingTabHelper::
        CreateForWebContents(web_contents);
  }
#endif
  ExternalProtocolObserver::CreateForWebContents(web_contents);
  favicon::CreateContentFaviconDriverForWebContents(web_contents);
  FileSystemAccessPermissionRequestManager::CreateForWebContents(web_contents);
  FileSystemAccessTabHelper::CreateForWebContents(web_contents);
  FindBarState::ConfigureWebContents(web_contents);
  download::DownloadNavigationObserver::CreateForWebContents(
      web_contents,
      download::NavigationMonitorFactory::GetForKey(profile->GetProfileKey()));
  history::WebContentsTopSitesObserver::CreateForWebContents(
      web_contents, TopSitesFactory::GetForProfile(profile).get());
  HistoryTabHelper::CreateForWebContents(web_contents);
  HistoryClustersTabHelper::CreateForWebContents(web_contents);
  HistoryEmbeddingsTabHelper::CreateForWebContents(web_contents);
  HttpsOnlyModeTabHelper::CreateForWebContents(web_contents);
  webapps::InstallableManager::CreateForWebContents(web_contents);
  login_detection::LoginDetectionTabHelper::MaybeCreateForWebContents(
      web_contents);
  if (MediaEngagementService::IsEnabled()) {
    MediaEngagementService::CreateWebContentsObserver(web_contents);
  }
  if (auto* metrics_services_manager =
          g_browser_process->GetMetricsServicesManager()) {
    metrics::MetricsServicesWebContentsObserver::CreateForWebContents(
        web_contents, metrics_services_manager->GetOnDidStartLoadingCb(),
        metrics_services_manager->GetOnDidStopLoadingCb(),
        metrics_services_manager->GetOnRendererUnresponsiveCb());
  }
  MixedContentSettingsTabHelper::CreateForWebContents(web_contents);
  NavigationMetricsRecorder::CreateForWebContents(web_contents);
  NavigationPredictorPreconnectClient::CreateForWebContents(web_contents);
  OpenerHeuristicTabHelper::CreateForWebContents(web_contents);
  if (optimization_guide::features::IsOptimizationHintsEnabled()) {
    OptimizationGuideWebContentsObserver::CreateForWebContents(web_contents);
  }
  page_content_annotations::PageContentAnnotationsService*
      page_content_annotations_service =
          PageContentAnnotationsServiceFactory::GetForProfile(profile);
  if (page_content_annotations_service) {
    page_content_annotations::PageContentAnnotationsWebContentsObserver::
        CreateForWebContents(web_contents);

#if BUILDFLAG(IS_ANDROID)
    // If enabled, save sensitivity data for each non-incognito non-custom
    // android tab
    // TODO(crbug.com/40276584): Consider moving check conditions or the
    // registration logic to sensitivity_persisted_tab_data_android.*
    if (!profile->IsOffTheRecord()) {
      if (auto* tab = TabAndroid::FromWebContents(web_contents);
          (tab && !tab->IsCustomTab())) {
        SensitivityPersistedTabDataAndroid::From(
            tab,
            base::BindOnce(
                [](page_content_annotations::PageContentAnnotationsService*
                       page_content_annotations_service,
                   PersistedTabDataAndroid* persisted_tab_data) {
                  auto* sensitivity_persisted_tab_data_android =
                      static_cast<SensitivityPersistedTabDataAndroid*>(
                          persisted_tab_data);
                  sensitivity_persisted_tab_data_android->RegisterPCAService(
                      page_content_annotations_service);
                },
                page_content_annotations_service));
      }
    }
#endif  // BUILDFLAG(IS_ANDROID)
  }
  chrome::InitializePageLoadMetricsForWebContents(web_contents);
  if (auto* pm_registry =
          performance_manager::PerformanceManagerRegistry::GetInstance()) {
    pm_registry->SetPageType(web_contents, performance_manager::PageType::kTab);
  }
  permissions::PermissionRequestManager::CreateForWebContents(web_contents);
  permissions::PermissionRecoverySuccessRateTracker::CreateForWebContents(
      web_contents);
  // The PopupBlockerTabHelper has an implicit dependency on
  // ChromeSubresourceFilterClient being available in its constructor.
  blocked_content::PopupBlockerTabHelper::CreateForWebContents(web_contents);
  blocked_content::PopupOpenerTabHelper::CreateForWebContents(
      web_contents, base::DefaultTickClock::GetInstance(),
      HostContentSettingsMapFactory::GetForProfile(profile));
  if (predictors::LoadingPredictorFactory::GetForProfile(profile)) {
    predictors::LoadingPredictorTabHelper::CreateForWebContents(web_contents);
  }
  PrefsTabHelper::CreateForWebContents(web_contents);
  prerender::NoStatePrefetchTabHelper::CreateForWebContents(web_contents);
  RecentlyAudibleHelper::CreateForWebContents(web_contents);
  RedirectHeuristicTabHelper::CreateForWebContents(web_contents);
#if BUILDFLAG(IS_ANDROID)
  RequestDesktopSiteWebContentsObserverAndroid::CreateForWebContents(
      web_contents);
#endif  // BUILDFLAG(IS_ANDROID)
  // TODO(siggi): Remove this once the Resource Coordinator refactoring is done.
  //     See https://crbug.com/910288.
  resource_coordinator::ResourceCoordinatorTabHelper::CreateForWebContents(
      web_contents);
  safe_browsing::SafeBrowsingNavigationObserver::MaybeCreateForWebContents(
      web_contents, HostContentSettingsMapFactory::GetForProfile(profile),
      safe_browsing::SafeBrowsingNavigationObserverManagerFactory::
          GetForBrowserContext(profile),
      profile->GetPrefs(), g_browser_process->safe_browsing_service());
  site_protection::SiteProtectionMetricsObserver::CreateForWebContents(
      web_contents);

  if (base::FeatureList::IsEnabled(
          safe_browsing::kTailoredSecurityIntegration)) {
    safe_browsing::TailoredSecurityUrlObserver::CreateForWebContents(
        web_contents,
        safe_browsing::TailoredSecurityServiceFactory::GetForProfile(profile));
  }
  if (base::FeatureList::IsEnabled(
          safe_browsing::kSafeBrowsingAsyncRealTimeCheck) &&
      g_browser_process->safe_browsing_service()) {
    safe_browsing::AsyncCheckTracker::CreateForWebContents(
        web_contents, g_browser_process->safe_browsing_service()->ui_manager(),
        safe_browsing::AsyncCheckTracker::
            IsPlatformEligibleForSyncCheckerCheckAllowlist());
  }
  // SafeBrowsingTabObserver creates a ClientSideDetectionHost, which observes
  // events from PermissionRequestManager and AsyncCheckTracker in its
  // constructor. Therefore, PermissionRequestManager and AsyncCheckTracker need
  // to be created before SafeBrowsingTabObserver is created.
  safe_browsing::SafeBrowsingTabObserver::CreateForWebContents(
      web_contents,
      std::make_unique<safe_browsing::ChromeSafeBrowsingTabObserverDelegate>());
  safe_browsing::TriggerCreator::MaybeCreateTriggersForWebContents(
      profile, web_contents);
  SafetyTipWebContentsObserver::CreateForWebContents(web_contents);
  SearchEngineTabHelper::CreateForWebContents(web_contents);
  if (site_engagement::SiteEngagementService::IsEnabled()) {
    site_engagement::SiteEngagementService::Helper::CreateForWebContents(
        web_contents,
        prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(
            profile));
  }
  SoundContentSettingObserver::CreateForWebContents(web_contents);
  StorageAccessAPITabHelper::CreateForWebContents(
      web_contents, StorageAccessAPIServiceFactory::GetForBrowserContext(
                        web_contents->GetBrowserContext()));
  // Do not create for Incognito mode.
  if (!profile->IsOffTheRecord()) {
    SupervisedUserNavigationObserver::CreateForWebContents(web_contents);
  }
  HttpErrorTabHelper::CreateForWebContents(web_contents);
  sync_sessions::SyncSessionsRouterTabHelper::CreateForWebContents(
      web_contents,
      sync_sessions::SyncSessionsWebContentsRouterFactory::GetForProfile(
          profile));
  TabUIHelper::CreateForWebContents(web_contents);
  tasks::TaskTabHelper::CreateForWebContents(web_contents);
  tpcd::metadata::TpcdMetadataDevtoolsObserver::CreateForWebContents(
      web_contents);
  tpcd::trial::ValidityService::MaybeCreateForWebContents(web_contents);
  TrustedVaultEncryptionKeysTabHelper::CreateForWebContents(web_contents);
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kSafetyHub)) {
    auto* service = UnusedSitePermissionsServiceFactory::GetForProfile(profile);
    if (service) {
      UnusedSitePermissionsService::TabHelper::CreateForWebContents(
          web_contents, service);
    }
  }
#else   // BUILDFLAG(IS_ANDROID)
  auto* service = UnusedSitePermissionsServiceFactory::GetForProfile(profile);
  if (service) {
    UnusedSitePermissionsService::TabHelper::CreateForWebContents(web_contents,
                                                                  service);
  }
#endif  // BUILDFLAG(IS_ANDROID)
  ukm::InitializeSourceUrlRecorderForWebContents(web_contents);
  v8_compile_hints::V8CompileHintsTabHelper::MaybeCreateForWebContents(
      web_contents);
  vr::VrTabHelper::CreateForWebContents(web_contents);
  OneTimePermissionsTrackerHelper::CreateForWebContents(web_contents);

  // NO! Do not just add your tab helper here. This is a large alphabetized
  // block; please insert your tab helper above in alphabetical order.

  // --- Section 2: Platform-specific tab helpers ---

#if BUILDFLAG(IS_ANDROID)
  webapps::MLInstallabilityPromoter::CreateForWebContents(web_contents);
  {
    // Remove after fixing https://crbug/905919
    TRACE_EVENT0("browser", "AppBannerManagerAndroid::CreateForWebContents");
    webapps::AppBannerManagerAndroid::CreateForWebContents(
        web_contents, std::make_unique<webapps::ChromeAppBannerManagerAndroid>(
                          *web_contents));
  }
  ContextMenuHelper::CreateForWebContents(web_contents);
  FastCheckoutTabHelper::CreateForWebContents(web_contents);

  javascript_dialogs::TabModalDialogManager::CreateForWebContents(
      web_contents,
      std::make_unique<JavaScriptTabModalDialogManagerDelegateAndroid>(
          web_contents));
  if (OomInterventionTabHelper::IsEnabled()) {
    OomInterventionTabHelper::CreateForWebContents(web_contents);
  }
  PolicyAuditorBridge::CreateForWebContents(web_contents);
  PluginObserverAndroid::CreateForWebContents(web_contents);

  if (base::FeatureList::IsEnabled(payments::facilitated::kEnablePixPayments) ||
      base::FeatureList::IsEnabled(blink::features::kPaymentLinkDetection)) {
    if (auto* optimization_guide_decider =
            OptimizationGuideKeyedServiceFactory::GetForProfile(profile)) {
      ChromeFacilitatedPaymentsClient::CreateForWebContents(
          web_contents, optimization_guide_decider);
    }
  }
#else  // BUILDFLAG(IS_ANDROID)
  if (web_app::AreWebAppsUserInstallable(profile)) {
    webapps::MLInstallabilityPromoter::CreateForWebContents(web_contents);
    webapps::AppBannerManagerDesktop::CreateForWebContents(web_contents);
  }
  if (base::FeatureList::IsEnabled(
          blink::features::kMediaSessionEnterPictureInPicture)) {
    AutoPictureInPictureTabHelper::CreateForWebContents(web_contents);
  }
  BookmarkTabHelper::CreateForWebContents(web_contents);
  BrowserSyncedTabDelegate::CreateForWebContents(web_contents);
  FocusTabAfterNavigationHelper::CreateForWebContents(web_contents);
  FormInteractionTabHelper::CreateForWebContents(web_contents);
  FramebustBlockTabHelper::CreateForWebContents(web_contents);
  IntentPickerTabHelper::CreateForWebContents(web_contents);
  javascript_dialogs::TabModalDialogManager::CreateForWebContents(
      web_contents,
      std::make_unique<JavaScriptTabModalDialogManagerDelegateDesktop>(
          web_contents));
  ManagePasswordsUIController::CreateForWebContents(web_contents);
  if (PrivacySandboxPromptHelper::ProfileRequiresPrompt(profile)) {
    PrivacySandboxPromptHelper::CreateForWebContents(web_contents);
  }

  if (SearchEngineChoiceTabHelper::IsHelperNeeded()) {
    SearchEngineChoiceTabHelper::CreateForWebContents(web_contents);
  }

  SadTabHelper::CreateForWebContents(web_contents);
  SearchTabHelper::CreateForWebContents(web_contents);
  TabDialogs::CreateForWebContents(web_contents);
  MemorySaverChipTabHelper::CreateForWebContents(web_contents);
  TabResourceUsageTabHelper::CreateForWebContents(web_contents);
  if (base::FeatureList::IsEnabled(features::kTabHoverCardImages) ||
      base::FeatureList::IsEnabled(features::kWebUITabStrip)) {
    ThumbnailTabHelper::CreateForWebContents(web_contents);
  }
  chrome::UMABrowsingActivityObserver::TabHelper::CreateForWebContents(
      web_contents);
  web_modal::WebContentsModalDialogManager::CreateForWebContents(web_contents);
  if (OmniboxFieldTrial::IsZeroSuggestPrefetchingEnabled()) {
    ZeroSuggestPrefetchTabHelper::CreateForWebContents(web_contents);
  }
  if (commerce::isContextualConsentEnabled()) {
    commerce_hint::CommerceHintTabHelper::CreateForWebContents(web_contents);
  }
  if (companion::IsCompanionFeatureEnabled()) {
    companion::CompanionTabHelper::CreateForWebContents(web_contents);
  }
  if (base::FeatureList::IsEnabled(
          companion::features::internal::
              kCompanionEnabledByObservingExpsNavigations)) {
    companion::ExpsRegistrationSuccessObserver::CreateForWebContents(
        web_contents);
  }
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_COMPOSE)
  // We need to create the ChromeComposeClient to listen for the feature
  // being turned on, even if it is not enabled yet.
  if (!profile->IsOffTheRecord()) {
    ChromeComposeClient::CreateForWebContents(web_contents);
  }
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  GoogleOneOfferIphTabHelper::CreateForWebContents(web_contents);
  // Do not create for Incognito mode.
  if (!profile->IsOffTheRecord()) {
    CampaignsManagerSessionTabHelper::CreateForWebContents(web_contents);
  }
  ash::BootTimesRecorderTabHelper::MaybeCreateForWebContents(web_contents);
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  WebContentsCanGoBackObserver::CreateForWebContents(web_contents);
#endif

#if BUILDFLAG(IS_CHROMEOS)
  ContainerAppTabHelper::MaybeCreateForWebContents(web_contents);
  CrosAppsTabHelper::MaybeCreateForWebContents(web_contents);
  mahi::MahiTabHelper::MaybeCreateForWebContents(web_contents);
  policy::DlpContentTabHelper::MaybeCreateForWebContents(web_contents);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  webapps::PreRedirectionURLObserver::CreateForWebContents(web_contents);
#endif

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  metrics::DesktopSessionDurationObserver::CreateForWebContents(web_contents);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(
          features::kHappinessTrackingSurveysForDesktopDemo) ||
      base::FeatureList::IsEnabled(features::kTrustSafetySentimentSurvey) ||
      base::FeatureList::IsEnabled(features::kTrustSafetySentimentSurveyV2) ||
      base::FeatureList::IsEnabled(performance_manager::features::
                                       kPerformanceControlsPerformanceSurvey) ||
      base::FeatureList::IsEnabled(
          performance_manager::features::
              kPerformanceControlsBatteryPerformanceSurvey) ||
      base::FeatureList::IsEnabled(
          performance_manager::features::
              kPerformanceControlsMemorySaverOptOutSurvey) ||
      base::FeatureList::IsEnabled(
          performance_manager::features::
              kPerformanceControlsBatterySaverOptOutSurvey)) {
    HatsHelper::CreateForWebContents(web_contents);
  }
  SharedHighlightingPromo::CreateForWebContents(web_contents);
#endif

#if BUILDFLAG(IS_WIN)
  FontPrewarmerTabHelper::CreateForWebContents(web_contents);
#endif

#if defined(TOOLKIT_VIEWS)
  if (IsSideSearchEnabled(profile)) {
    SideSearchTabContentsHelper::CreateForWebContents(web_contents);
  }
#endif

  // --- Section 3: Feature tab helpers behind BUILDFLAGs ---
  // NOT for "if enabled"; put those in section 1.

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  captive_portal::CaptivePortalTabHelper::CreateForWebContents(
      web_contents, CaptivePortalServiceFactory::GetForProfile(profile),
      base::BindRepeating(
          &ChromeSecurityBlockingPageFactory::OpenLoginTabForWebContents,
          web_contents, false));
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // If the web contents already have a view type, don't overwrite it here. One
  // case where this can happen is when the user opens undocked developer tools.
  // For all developer tools web contents, the view type is set to
  // `kDeveloperTools` by the `DevToolsWindow` before tab helpers are attached.
  if (extensions::GetViewType(web_contents) ==
      extensions::mojom::ViewType::kInvalid) {
    extensions::SetViewType(web_contents,
                            extensions::mojom::ViewType::kTabContents);
  }

  extensions::TabHelper::CreateForWebContents(web_contents);
  extensions::NavigationExtensionEnabler::CreateForWebContents(web_contents);

  extensions::WebNavigationTabObserver::CreateForWebContents(web_contents);
  if (web_app::AreWebAppsEnabled(profile)) {
    web_app::WebAppTabHelper::CreateForWebContents(web_contents);
  }
  // Note WebAppMetricsTabHelper must be created after AppBannerManager.
  if (web_app::WebAppMetricsTabHelper::IsEnabled(web_contents)) {
    web_app::WebAppMetricsTabHelper::CreateForWebContents(web_contents);
  }
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  offline_pages::OfflinePageTabHelper::CreateForWebContents(web_contents);
  offline_pages::RecentTabHelper::CreateForWebContents(web_contents);
  offline_pages::AutoFetchPageLoadWatcher::CreateForWebContents(web_contents);
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
  HungPluginTabHelper::CreateForWebContents(web_contents);
  PluginObserver::CreateForWebContents(web_contents);
#endif

// Only enable ChromeOS print preview if `kPrintPreviewCrosPrimary` is enabled
// and is a ChromeOS build. Otherwise instantiate browser print preview.
#if BUILDFLAG(ENABLE_PRINTING) && BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(::features::kPrintPreviewCrosPrimary)) {
    chromeos::printing::InitializePrintingForWebContents(web_contents);
  } else {
    printing::InitializePrintingForWebContents(web_contents);
  }
#elif BUILDFLAG(ENABLE_PRINTING)
  printing::InitializePrintingForWebContents(web_contents);
#endif

  // --- Section 4: The warning ---

  // NONO    NO   NONONO   !
  // NO NO   NO  NO    NO  !
  // NO  NO  NO  NO    NO  !
  // NO   NO NO  NO    NO  !
  // NO    NONO   NONONO   !

  // Do NOT just drop your tab helpers here! There are three sections above (1.
  // All platforms, 2. Some platforms, 3. Behind BUILDFLAGs). Each is in rough
  // alphabetical order. PLEASE PLEASE PLEASE add your flag to the correct
  // section in the correct order.

  // This is common code for all of us. PLEASE DO YOUR PART to keep it tidy and
  // organized.
}
