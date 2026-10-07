// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_helpers.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/breadcrumbs/breadcrumb_manager_tab_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/mixed_content_settings_tab_helper.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/enterprise/connectors/referrer_cache_utils.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/file_system_access/file_system_access_features.h"
#include "chrome/browser/file_system_access/file_system_access_permission_request_manager.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/history_clusters/history_clusters_tab_helper.h"
#include "chrome/browser/login_detection/login_detection_tab_helper.h"
#include "chrome/browser/lookalikes/safety_tip_web_contents_observer.h"
#include "chrome/browser/optimization_guide/optimization_guide_web_contents_observer.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_initialize.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_helper.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/predictors/loading_predictor_tab_helper.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/browser/ssl/https_only_mode_tab_helper.h"
#include "chrome/browser/subresource_filter/chrome_content_subresource_filter_web_contents_helper_factory.h"
#include "chrome/browser/sync/sessions/sync_sessions_router_tab_helper.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/trusted_vault/trusted_vault_encryption_keys_tab_helper.h"
#include "chrome/browser/ui/autofill/autofill_client_provider.h"
#include "chrome/browser/ui/autofill/autofill_client_provider_factory.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/blocked_content/popup_blocker_tab_helper.h"
#include "components/breadcrumbs/core/breadcrumbs_status.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/page_content_annotations/content/annotate_page_content_request.h"
#include "components/page_content_annotations/core/page_content_extraction_types.h"
#include "components/page_info/core/features.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "components/performance_manager/public/features.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_recovery_success_rate_tracker.h"
#include "components/permissions/permission_request_manager.h"
#include "components/safe_browsing/content/browser/async_check_tracker.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/search/ntp_features.h"
#include "components/search/search.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/site_engagement/content/site_engagement_helper.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/tabs/public/tab_interface.h"
#include "components/tracing/common/tracing_switches.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "media/base/media_switches.h"
#include "pdf/buildflags.h"
#include "rlz/buildflags/buildflags.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/webui/buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/content_settings/request_desktop_site_web_contents_observer_android.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/ui/android/context_menu_helper.h"
#include "chrome/browser/ui/javascript_dialogs/javascript_tab_modal_dialog_manager_delegate_android.h"
#include "content/public/common/content_features.h"
#else
#include "chrome/browser/ui/javascript_dialogs/javascript_tab_modal_dialog_manager_delegate_desktop.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/extensions/api/web_navigation/web_navigation_tab_observer.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/mojom/view_type.mojom.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "chrome/browser/safe_browsing/chrome_password_reuse_detection_manager_client.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/trigger_creator.h"
#endif

using content::WebContents;

namespace {

const char kTabContentsAttachedTabHelpersUserDataKey[] =
    "TabContentsAttachedTabHelpers";

std::optional<int64_t> GetPageContentAnnotationsTabId(
    content::WebContents* web_contents) {
#if BUILDFLAG(IS_ANDROID)
  if (TabAndroid* tab = TabAndroid::FromWebContents(web_contents)) {
    return tab->GetAndroidId();
  }
#else
  SessionID id = sessions::SessionTabHelper::IdForTab(web_contents);
  if (id.is_valid()) {
    return id.id();
  }
#endif
  return std::nullopt;
}

}  // namespace

// static
// WARNING: Do not use this class for desktop chrome. Use TabFeatures instead.
// See
// https://chromium.googlesource.com/chromium/src/+/main/docs/chrome_browser_design_principles.md
void TabHelpers::AttachTabHelpers(WebContents* web_contents,
                                  bool enable_browser_autofill) {
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

  // ZoomController comes before common tab helpers since ChromeAutofillClient
  // may want to register as a ZoomObserver with it.
  zoom::ZoomController::CreateForWebContents(web_contents);

  // infobars::ContentInfoBarManager comes before common tab helpers since
  // ChromeSubresourceFilterClient has it as a dependency.
  infobars::ContentInfoBarManager::CreateForWebContents(web_contents);

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  // --- Section 1: Common tab helpers ---
  // AutofillClientProvider initializes ContentAutofillClient for web_contents,
  // which is gated by enable_browser_autofill.
  if (enable_browser_autofill) {
    autofill::AutofillClientProvider& autofill_client_provider =
        autofill::AutofillClientProviderFactory::GetForProfile(profile);
    autofill_client_provider.CreateClientForWebContents(web_contents);
  }

  if (breadcrumbs::IsEnabled(g_browser_process->local_state())) {
    BreadcrumbManagerTabHelper::CreateForWebContents(web_contents);
  }
  // Password manager and password reuse detection rely on ChromeAutofillClient
  // initialized by browser autofill, which is gated by enable_browser_autofill.
  if (enable_browser_autofill) {
    autofill::AutofillClientProvider& autofill_client_provider =
        autofill::AutofillClientProviderFactory::GetForProfile(profile);
    if (!autofill_client_provider.uses_platform_autofill()) {
      ChromePasswordManagerClient::CreateForWebContents(web_contents);
    }
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
    ChromePasswordReuseDetectionManagerClient::CreateForWebContents(
        web_contents);
#endif
  }
  CreateSubresourceFilterWebContentsHelper(web_contents);
  ChromeTranslateClient::CreateForWebContents(web_contents);
  content_settings::PageSpecificContentSettings::CreateForWebContents(
      web_contents,
      std::make_unique<PageSpecificContentSettingsDelegate>(web_contents));
  favicon::CreateContentFaviconDriverForWebContents(web_contents);
  FileSystemAccessPermissionRequestManager::CreateForWebContents(web_contents);
  if (!profile->IsOffTheRecord()) {
    auto* history_tab_helper =
        HistoryTabHelper::GetOrCreateForWebContents(web_contents);
    HistoryClustersTabHelper::CreateForWebContents(web_contents,
                                                   history_tab_helper);
  }
  HttpsOnlyModeTabHelper::CreateForWebContents(web_contents);
  webapps::InstallableManager::CreateForWebContents(web_contents);
  login_detection::LoginDetectionTabHelper::MaybeCreateForWebContents(
      web_contents);
  MixedContentSettingsTabHelper::CreateForWebContents(web_contents);
  if (optimization_guide::features::IsOptimizationHintsEnabled()) {
    OptimizationGuideWebContentsObserver::CreateForWebContents(web_contents);
  }
  page_content_annotations::PageContentAnnotationsService*
      page_content_annotations_service =
          PageContentAnnotationsServiceFactory::GetForProfile(profile);
  if (page_content_annotations_service) {
    // TODO(b/478883979): Consider decoupling this from
    // PageContentAnnotationsService.
    auto* page_content_extraction_service = page_content_annotations::
        PageContentExtractionServiceFactory::GetForProfile(profile);
    if (page_content_extraction_service) {
      page_content_annotations::AnnotatedPageContentRequest::
          CreateForWebContents(
              web_contents, *page_content_extraction_service,
              base::BindRepeating(&page_content_annotations::FetchPageContext),
              base::BindRepeating(&GetPageContentAnnotationsTabId));
    }
  }
  InitializePageLoadMetricsForWebContents(web_contents);
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
  if (predictors::LoadingPredictorFactory::GetForProfile(profile)) {
    predictors::LoadingPredictorTabHelper::CreateForWebContents(web_contents);
  }
  PrefsTabHelper::CreateForWebContents(web_contents);
#if BUILDFLAG(IS_ANDROID)
  RequestDesktopSiteWebContentsObserverAndroid::CreateForWebContents(
      web_contents);
#endif  // BUILDFLAG(IS_ANDROID)
  // TODO(siggi): Remove this once the Resource Coordinator refactoring is done.
  //     See https://crbug.com/40604438.
  resource_coordinator::ResourceCoordinatorTabHelper::CreateForWebContents(
      web_contents);
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  safe_browsing::SafeBrowsingNavigationObserver::MaybeCreateForWebContents(
      web_contents, HostContentSettingsMapFactory::GetForProfile(profile),
      safe_browsing::SafeBrowsingNavigationObserverManagerFactory::
          GetForBrowserContext(profile),
      profile->GetPrefs(), g_browser_process->safe_browsing_service(),
      enterprise_connectors::IsReferrerChainNeededForEnterprise(profile));
#endif

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  if (g_browser_process->safe_browsing_service()) {
    safe_browsing::AsyncCheckTracker::CreateForWebContents(
        web_contents, g_browser_process->safe_browsing_service()->ui_manager(),
        safe_browsing::AsyncCheckTracker::
            IsPlatformEligibleForSyncCheckerCheckAllowlist());
  }
  safe_browsing::TriggerCreator::MaybeCreateTriggersForWebContents(
      profile, web_contents);
#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  SafetyTipWebContentsObserver::CreateForWebContents(web_contents);
  if (site_engagement::SiteEngagementService::IsEnabled()) {
    site_engagement::SiteEngagementService::Helper::CreateForWebContents(
        web_contents,
        prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(
            profile));
  }
  TrustedVaultEncryptionKeysTabHelper::CreateForWebContents(web_contents);
  ukm::InitializeSourceUrlRecorderForWebContents(web_contents);
  OneTimePermissionsTrackerHelper::CreateForWebContents(web_contents);

  // NO! Do not just add your tab helper here. This is a large alphabetized
  // block; please insert your tab helper above in alphabetical order.

  // --- Section 2: Platform-specific tab helpers ---

#if BUILDFLAG(IS_ANDROID)
  ContextMenuHelper::CreateForWebContents(web_contents);

  javascript_dialogs::TabModalDialogManager::CreateForWebContents(
      web_contents,
      std::make_unique<JavaScriptTabModalDialogManagerDelegateAndroid>(
          web_contents));
#else   // BUILDFLAG(IS_ANDROID)
  javascript_dialogs::TabModalDialogManager::CreateForWebContents(
      web_contents,
      std::make_unique<JavaScriptTabModalDialogManagerDelegateDesktop>(
          web_contents));

  web_modal::WebContentsModalDialogManager::CreateForWebContents(web_contents);
#endif  // BUILDFLAG(IS_ANDROID)

  // --- Section 3: Feature tab helpers behind BUILDFLAGs ---
  // NOT for "if enabled"; put those in section 1.

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // If the web contents already have a view type, don't overwrite it here. One
  // case where this can happen is when the user opens undocked developer tools.
  // For all developer tools web contents, the view type is set to
  // `kDeveloperTools` by the `DevToolsWindow` before tab helpers are attached.
  if (extensions::GetViewType(web_contents) ==
      extensions::mojom::ViewType::kInvalid) {
    extensions::SetViewType(web_contents,
                            extensions::mojom::ViewType::kTabContents);
  }
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // These helpers are used on Win/Mac/Linux and also desktop Android.
  extensions::WebNavigationTabObserver::CreateForWebContents(web_contents);
  extensions::TabHelper::CreateForWebContents(web_contents);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

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
