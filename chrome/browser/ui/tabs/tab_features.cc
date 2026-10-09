// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/public/tab_features.h"

#include <memory>

#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_surface_tab_helper.h"
#include "chrome/browser/actor/actor_tab_data.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"
#include "chrome/browser/banners/app_banner_manager_desktop.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/breadcrumbs/breadcrumb_manager_tab_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chained_back_navigation_tracker.h"
#include "chrome/browser/commerce/in_stock_notification/in_stock_notification_manager.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/complex_tasks/task_tab_helper.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/mixed_content_settings_tab_helper.h"
#include "chrome/browser/content_settings/sound_content_setting_observer.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_controller.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_web_contents_observer.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/contextual_search/contextual_search_cue_tab_state.h"
#include "chrome/browser/contextual_search/contextual_search_cue_target.h"
#include "chrome/browser/enterprise/data_protection/data_protection_navigation_controller.h"
#include "chrome/browser/enterprise/net/enterprise_proxy_error_service_factory.h"
#include "chrome/browser/enterprise/reporting/saas_usage/saas_usage_navigation_observer.h"
#include "chrome/browser/external_protocol/external_protocol_observer.h"
#include "chrome/browser/facilitated_payments/ui/chrome_facilitated_payments_client.h"
#include "chrome/browser/file_system_access/file_system_access_permission_request_manager.h"
#include "chrome/browser/file_system_access/file_system_access_tab_helper.h"
#include "chrome/browser/glic/host/context/glic_page_features_manager.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_helper.h"
#include "chrome/browser/glic/suggestions/glic_cue_tab_state.h"
#include "chrome/browser/glic/suggestions/glic_cue_target.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/history_clusters/history_clusters_tab_helper.h"
#include "chrome/browser/history_embeddings/history_embeddings_service_factory.h"
#include "chrome/browser/history_embeddings/history_embeddings_tab_helper.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/indigo/indigo_cue_target.h"
#include "chrome/browser/indigo/indigo_page_action_controller.h"
#include "chrome/browser/loader/from_gws_navigation_and_keep_alive_request_observer.h"
#include "chrome/browser/login_detection/login_detection_tab_helper.h"
#include "chrome/browser/lookalikes/safety_tip_web_contents_observer.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/browser/metrics/variations/google_groups_manager_factory.h"
#include "chrome/browser/multistep_filter/chrome_filter_navigation_observer.h"
#include "chrome/browser/multistep_filter/ui/filter_ui_controller.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_preconnect_client.h"
#include "chrome/browser/net/http_auth_cache_status.h"
#include "chrome/browser/net/net_error_tab_helper.h"
#include "chrome/browser/net/qwac_web_contents_observer.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/page_info/about_this_site_tab_helper.h"
#include "chrome/browser/page_info/page_info_features.h"
#include "chrome/browser/payments/web_payments_observer.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_helper.h"
#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_helper.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/predictors/loading_predictor_tab_helper.h"
#include "chrome/browser/preloading/bookmarkbar_preload/bookmarkbar_preload_pipeline_manager.h"
#include "chrome/browser/preloading/new_tab_page_preload/new_tab_page_preload_pipeline_manager.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_tab_helper.h"
#include "chrome/browser/preloading/prefetch/zero_suggest_prefetch/zero_suggest_prefetch_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/site_protection/site_protection_metrics_observer.h"
#include "chrome/browser/ssl/ask_before_http_dialog_controller.h"
#include "chrome/browser/ssl/connection_help_tab_helper.h"
#include "chrome/browser/ssl/https_only_mode_tab_helper.h"
#include "chrome/browser/ssl/security_state_event_observer.h"
#include "chrome/browser/storage_access_api/storage_access_api_service_factory.h"
#include "chrome/browser/storage_access_api/storage_access_api_service_impl.h"
#include "chrome/browser/storage_access_api/storage_access_api_tab_helper.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#include "chrome/browser/sync/sessions/sync_sessions_router_tab_helper.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync_tab_context/tab_context_decryption_token_tab_helper.h"
#include "chrome/browser/tab_contents/form_interaction_tab_helper.h"
#include "chrome/browser/tab_contents/navigation_metrics_recorder.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/trusted_vault/trusted_vault_encryption_keys_tab_helper.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/autofill/bubble_manager.h"
#include "chrome/browser/ui/autofill/one_time_tokens/gmail_otp_opt_in_bubble_controller.h"
#include "chrome/browser/ui/autofill/payments/omnibox_autofill_bubble_controller.h"
#include "chrome/browser/ui/autofill/payments/omnibox_autofill_page_action_controller.h"
#include "chrome/browser/ui/autofill/payments/payments_churned_users_bubble_controller.h"
#include "chrome/browser/ui/autofill/payments/payments_churned_users_page_action_controller.h"
#include "chrome/browser/ui/autofill/payments/wallet_reminder_notice_bubble_controller.h"
#include "chrome/browser/ui/autofill/payments/wallet_reminder_notice_page_action_controller.h"
#include "chrome/browser/ui/blocked_content/framebust_block_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/commerce/commerce_ui_tab_helper.h"
#include "chrome/browser/ui/context_highlight/context_highlight_tab_feature.h"
#include "chrome/browser/ui/extensions/extension_side_panel_manager.h"
#include "chrome/browser/ui/find_bar/find_bar_state.h"
#include "chrome/browser/ui/focus_tab_after_navigation_helper.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/javascript_dialogs/javascript_tab_modal_dialog_manager_delegate_desktop.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/page_action/action_ids.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/page_action/page_action_properties_provider.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/performance_controls/memory_saver_chip_controller.h"
#include "chrome/browser/ui/performance_controls/memory_saver_chip_tab_helper.h"
#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/safety_hub/revoked_permissions_service.h"
#include "chrome/browser/ui/safety_hub/revoked_permissions_service_factory.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/search_engine_choice/search_engine_choice_tab_helper.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/sync/browser_synced_tab_delegate.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/alert/child_tab_alert_helper.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/back_to_opener/back_to_opener_controller.h"
#include "chrome/browser/ui/tabs/inactive_window_mouse_event_controller.h"
#include "chrome/browser/ui/tabs/page_context_eligibility_helper.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_page_action_controller.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_tab_data.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_on_close_helper.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_web_contents_listener.h"
#include "chrome/browser/ui/tabs/tab_creation_metrics_controller.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/thumbnails/thumbnail_tab_helper.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_translate_action_listener.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/uma_browsing_activity_observer.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_page_action_controller.h"
#include "chrome/browser/ui/views/commerce/discounts_page_action_view_controller.h"
#include "chrome/browser/ui/views/commerce/price_insights_page_action_view_controller.h"
#include "chrome/browser/ui/views/file_system_access/file_system_access_page_action_controller.h"
#include "chrome/browser/ui/views/intent_picker/intent_picker_view_page_action_controller.h"
#include "chrome/browser/ui/views/js_optimization/js_optimizations_page_action_controller.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_page_action_controller.h"
#include "chrome/browser/ui/views/location_bar/lens_overlay_homework_page_action_controller.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_page_action_controller.h"
#include "chrome/browser/ui/views/side_panel/customize_chrome/side_panel_controller_views.h"
#include "chrome/browser/ui/views/tab_sharing/tab_capture_contents_border_helper.h"
#include "chrome/browser/ui/views/translate/translate_page_action_controller.h"
#include "chrome/browser/ui/views/zoom/zoom_view_controller.h"
#include "chrome/browser/ui/web_applications/pwa_install_page_action.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/ui/webui_browser/webui_browser.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/contextual_tasks/public/features.h"
#include "components/download/content/factory/navigation_monitor_factory.h"
#include "components/download/content/public/download_navigation_observer.h"
#include "components/enterprise/browser/reporting/reporting_features.h"
#include "components/enterprise/net/content/enterprise_proxy_tab_helper.h"
#include "components/history/content/browser/web_contents_top_sites_observer.h"
#include "components/history/core/browser/top_sites.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager.h"
#include "components/multistep_filter/core/features.h"
#include "components/payments/core/features.h"
#include "components/search/search.h"
#include "components/site_engagement/content/site_engagement_helper.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/skills/features.h"
#include "content/public/browser/navigation_controller.h"
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_observer.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/contextual_tasks/contextual_tasks_tab_visit_tracker.h"
#include "chrome/browser/contextual_tasks/copy_search_journey_tab_feature.h"
#include "chrome/browser/record_replay/chrome_record_replay_client.h"
#include "chrome/browser/ui/views/location_bar/record_replay_page_action_controller.h"
#include "chrome/browser/wallet/chrome_walletable_pass_client.h"
#include "components/record_replay/core/common/record_replay_features.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/metrics/oom/commit_limit_oom_recovery_tracker.h"
#include "chrome/browser/ui/search_promotion/search_promotion_navigation_observer.h"
#include "components/feature_engagement/public/feature_constants.h"
#endif
#include "chrome/browser/glic/browser_ui/glic_tab_indicator_helper.h"
#include "chrome/browser/glic/glic_marketing_page_tab_helper.h"
#include "chrome/browser/glic/glic_promotion_source_navigation_observer.h"
#include "chrome/browser/glic/glic_selection_observer.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/widget/glic_side_panel_coordinator_impl.h"
#include "chrome/browser/glic/selection/selection_overlay_controller.h"
#include "chrome/browser/glic/selection/selection_suggestion_tool.h"
#include "chrome/browser/glic/service/glic_instance_helper.h"
#include "chrome/browser/selection/suggestion_service.h"
#include "chrome/browser/skills/skills_ui_tab_controller.h"
#include "chrome/browser/skills/skills_update_observer.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_attachment_tracker.h"
#include "chrome/browser/v8_compile_hints/v8_compile_hints_tab_helper.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/window_management/window_management_content_setting_observer.h"
#include "chrome/browser/web_applications/policy/pre_redirection_url_observer.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/blocked_content/popup_blocker_tab_helper.h"
#include "components/blocked_content/popup_opener_tab_helper.h"
#include "components/breadcrumbs/core/breadcrumbs_status.h"
#include "components/client_hints/browser/client_hints_web_contents_observer.h"
#include "components/commerce/content/browser/commerce_tab_helper.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/facilitated_payments/core/features/features.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/metrics/content/metrics_services_web_contents_observer.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/page_content_annotations/content/page_content_annotations_web_contents_observer.h"
#include "components/passage_embeddings/core/passage_embeddings_features.h"
#include "components/permissions/permission_indicators_tab_data.h"
#include "components/permissions/permission_recovery_success_rate_tracker.h"
#include "components/permissions/permission_request_manager.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/security_interstitials/core/features.h"
#include "components/tabs/public/tab_interface.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/wallet/core/common/wallet_features.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/webapps/browser/installable/ml_installability_promoter.h"
#include "net/base/features.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/unowned_user_data/user_data_factory.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"  // nogncheck
#include "chrome/browser/ash/boot_times_recorder/boot_times_recorder_tab_helper.h"
#include "chrome/browser/ash/child_accounts/time_limits/web_time_navigation_observer.h"
#include "chrome/browser/ash/growth/campaigns_manager_session_tab_helper.h"
#include "chrome/browser/ash/mahi/web_contents/mahi_tab_helper.h"
#include "chrome/browser/chromeos/gemini_app/gemini_app_tab_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_tab_helper.h"
#include "chrome/browser/ui/ash/google_one/google_one_offer_iph_tab_helper.h"
#include "chrome/browser/ui/views/web_apps/protocol_handler_picker_coordinator.h"
#include "chromeos/ash/experiences/isolated_web_app/cros_isolated_web_app_enabler.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/hats/hats_helper.h"
#include "chrome/browser/ui/performance_controls/performance_controls_hats_service_factory.h"
#include "chrome/browser/ui/shared_highlighting/shared_highlighting_promo.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/font_prewarmer_tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_RLZ)
#include "chrome/browser/rlz/chrome_rlz_tracker_web_contents_observer.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/plugin_observer.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/contextual_tasks/search_ai_mode_promo_tab_helper.h"
#include "components/signin/public/base/signin_switches.h"
#endif

#include "components/captive_portal/core/buildflags.h"
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "chrome/browser/captive_portal/captive_portal_service_factory.h"
#include "chrome/browser/ssl/chrome_security_blocking_page_factory.h"
#include "components/captive_portal/content/captive_portal_tab_helper.h"
#endif

#include "components/compose/buildflags.h"
#if BUILDFLAG(ENABLE_COMPOSE)
#include "chrome/browser/compose/chrome_compose_client.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/app_tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/extensions/api/web_navigation/web_navigation_tab_observer.h"
#include "chrome/browser/extensions/navigation_extension_enabler.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/mojom/view_type.mojom.h"
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/offline_page_tab_helper.h"
#include "chrome/browser/offline_pages/recent_tab_helper.h"
#endif

#include "printing/buildflags/buildflags.h"
#if BUILDFLAG(ENABLE_PRINTING)
#include "chrome/browser/printing/printing_init.h"
#endif

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "chrome/browser/enterprise/connectors/referrer_cache_utils.h"
#include "chrome/browser/safe_browsing/chrome_password_reuse_detection_manager_client.h"
#include "chrome/browser/safe_browsing/chrome_safe_browsing_tab_observer_delegate.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/tailored_security/tailored_security_service_factory.h"
#include "chrome/browser/safe_browsing/tailored_security/tailored_security_url_observer.h"
#include "chrome/browser/safe_browsing/trigger_creator.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer.h"
#include "components/safe_browsing/content/browser/safe_browsing_tab_observer.h"
#include "components/safe_browsing/core/common/features.h"
#endif

namespace tabs {

TabFeatures::TabFeatures() = default;
TabFeatures::~TabFeatures() = default;

LensOverlayController* TabFeatures::lens_overlay_controller() {
  // LensSearchController won't exist on non-normal windows.
  return lens_search_controller_
             ? lens_search_controller_->lens_overlay_controller()
             : nullptr;
}

const LensOverlayController* TabFeatures::lens_overlay_controller() const {
  // LensSearchController won't exist on non-normal windows.
  return lens_search_controller_
             ? lens_search_controller_->lens_overlay_controller()
             : nullptr;
}

void TabFeatures::Init(TabInterface& tab, Profile* profile) {
  CHECK(!initialized_);
  initialized_ = true;

  // In tests you may want to disable TabFeatures initialization.
  // See tabs::TabModel::PreventFeatureInitializationForTesting
  CHECK(tab.GetBrowserWindowInterface());

  tab_subscriptions_.push_back(
      tab.RegisterWillDiscardContents(base::BindRepeating(
          &TabFeatures::WillDiscardContents, weak_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(webui::InitEmbeddingContext(&tab));

  // TODO(crbug.com/346148554): Do not create a SidePanelRegistry or
  // dependencies for non-normal browsers.
  side_panel_registry_ =
      GetUserDataFactory().CreateInstance<SidePanelRegistry>(tab, &tab);

  // Created before the page-action controllers below:
  // PwaInstallPageAction's constructor looks the manager up, and treats its
  // absence as a surface that cannot install web apps.
  if (web_app::AreWebAppsUserInstallable(profile)) {
    app_banner_manager_ =
        GetUserDataFactory()
            .CreateInstanceWithFactoryMethod<webapps::AppBannerManagerDesktop,
                                             tabs::TabInterface&,
                                             content::WebContents*>(
                tab, &webapps::AppBannerManagerDesktop::Create, tab,
                tab.GetContents());
    ml_installability_promoter_ =
        GetUserDataFactory().CreateInstance<webapps::MLInstallabilityPromoter>(
            tab, tab, tab.GetContents());
  }

  // This block instantiate the page action controllers. They do not require any
  // pre-condition. Because some feature need them during their instantiation,
  // therefore this block should come before the feature controllers
  // instantiation.
  auto* pinned_actions_model = PinnedToolbarActionsModel::Get(profile);
  CHECK(pinned_actions_model);
  page_action_controller_ =
      GetUserDataFactory()
          .CreateInstance<page_actions::PageActionControllerImpl>(
              tab, tab,
              page_actions::GetActivePageActionIds(
                  *tab.GetBrowserWindowInterface()),
              page_actions::PageActionPropertiesProvider(),
              pinned_actions_model);

  if (page_action_controller_->ActionExists(kActionShowTranslate)) {
    translate_page_action_controller_ =
        std::make_unique<TranslatePageActionController>(tab);
  }

  if (page_action_controller_->ActionExists(kActionShowMemorySaverChip)) {
    memory_saver_chip_controller_ =
        GetUserDataFactory()
            .CreateInstance<memory_saver::MemorySaverChipController>(
                tab, tab, *page_action_controller_);
  }

  if (page_action_controller_->ActionExists(kActionShowIntentPicker)) {
    intent_picker_view_page_action_controller_ =
        GetUserDataFactory()
            .CreateInstance<IntentPickerViewPageActionController>(tab, tab);
  }

  FileSystemAccessPermissionRequestManager::CreateForWebContents(
      tab.GetContents());
  if (page_action_controller_->ActionExists(kActionShowFileSystemAccess)) {
    file_system_access_page_action_controller_ =
        std::make_unique<FileSystemAccessPageActionController>(tab);
  }

  if (page_action_controller_->ActionExists(kActionShowZoomBubble)) {
    zoom_view_controller_ = std::make_unique<zoom::ZoomViewController>(
        tab, *page_action_controller_);
  }

  if (page_action_controller_->ActionExists(kActionInstallPwa)) {
    pwa_install_page_action_controller_ =
        std::make_unique<PwaInstallPageActionController>(
            tab, *page_action_controller_);
  }

  if (page_action_controller_->ActionExists(kActionCommercePriceInsights)) {
    commerce_price_insights_page_action_view_controller_ =
        GetUserDataFactory()
            .CreateInstance<commerce::PriceInsightsPageActionViewController>(
                tab, tab, *page_action_controller_);
  }

  tab_dialogs_ = TabDialogs::Create(tab, tab.GetContents());
  if (page_action_controller_->ActionExists(kActionShowPasswordsBubbleOrPage)) {
    manage_passwords_page_action_controller_ =
        std::make_unique<ManagePasswordsPageActionController>(
            *page_action_controller_);
  }

  if (page_action_controller_->ActionExists(kActionShowCookieControls)) {
    cookie_controls_page_action_controller_ =
        GetUserDataFactory().CreateInstance<CookieControlsPageActionController>(
            tab, tab, *profile, *page_action_controller_);
    cookie_controls_page_action_controller_->Init();
  }

  if (page_action_controller_->ActionExists(kActionLensOverlayHomework)) {
    lens_overlay_homework_page_action_controller_ =
        GetUserDataFactory()
            .CreateInstance<LensOverlayHomeworkPageActionController>(
                tab, tab, *profile, *page_action_controller_);
  }

  BookmarkTabHelper::CreateForWebContents(tab.GetContents());
  if (tab.GetBrowserWindowInterface()->GetType() ==
          BrowserWindowInterface::TYPE_NORMAL &&
      page_action_controller_->ActionExists(kActionBookmarkThisTab)) {
    bookmark_page_action_controller_ =
        GetUserDataFactory().CreateInstance<BookmarkPageActionController>(
            tab, tab, profile->GetPrefs(), *page_action_controller_);
  }

  if (base::FeatureList::IsEnabled(
          record_replay::features::kRecordReplayBase) &&
      page_action_controller_->ActionExists(kActionRecordReplay)) {
    record_replay_page_action_controller_ =
        GetUserDataFactory().CreateInstance<RecordReplayPageActionController>(
            tab, tab, *page_action_controller_);
  }

  if (page_action_controller_->ActionExists(kActionShowJsOptimizationsIcon)) {
    js_optimizations_page_action_controller_ =
        std::make_unique<JsOptimizationsPageActionController>(
            tab, *page_action_controller_);
  }

  page_context_eligibility_helper_ =
      GetUserDataFactory().CreateInstance<tabs::PageContextEligibilityHelper>(
          tab, tab);

  commerce_tab_helper_ = std::make_unique<commerce::CommerceTabHelper>(
      tab.GetContents(), profile->IsOffTheRecord(),
      commerce::ShoppingServiceFactory::GetForBrowserContext(profile),
      ISOLATED_WORLD_ID_CHROME_INTERNAL);

  // Features that are only enabled for normal browser windows. By default most
  // features should be instantiated in this block.
  if (tab.IsInNormalWindow()) {
    lens_search_controller_ =
        GetUserDataFactory().CreateInstance<LensSearchController>(tab, &tab);
    lens_search_controller_->Initialize(
        profile->GetVariationsClient(),
        IdentityManagerFactory::GetForProfile(profile), profile->GetPrefs(),
        SyncServiceFactory::GetForProfile(profile),
        ThemeServiceFactory::GetForProfile(profile));

    permission_indicators_tab_data_ =
        std::make_unique<permissions::PermissionIndicatorsTabData>(
            tab.GetContents());

    pinned_translate_action_listener_ =
        std::make_unique<PinnedTranslateActionListener>(&tab);

    if (!profile->IsPrimaryOTRProfileWithRegularParent()) {
      // TODO(crbug.com/40863325): Consider using the in-memory cache instead.
      commerce_ui_tab_helper_ =
          GetUserDataFactory().CreateInstance<commerce::CommerceUiTabHelper>(
              tab, tab,
              commerce::ShoppingServiceFactory::GetForBrowserContext(profile),
              BookmarkModelFactory::GetForBrowserContext(profile),
              ImageFetcherServiceFactory::GetForKey(profile->GetProfileKey())
                  ->GetImageFetcher(
                      image_fetcher::ImageFetcherConfig::kNetworkOnly),
              side_panel_registry_.get());
    }

    contextual_cueing_helper_ = glic::ContextualCueingHelper::MaybeCreate(&tab);
    glic_cue_tab_state_ = std::make_unique<glic::GlicCueTabState>(tab);
    contextual_search_cue_tab_state_ =
        std::make_unique<contextual_search::ContextualSearchCueTabState>(tab);

    if (tab_groups::TabGroupSyncService* tab_group_sync_service =
            tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile)) {
      saved_tab_group_web_contents_listener_ =
          std::make_unique<tab_groups::SavedTabGroupWebContentsListener>(
              tab_group_sync_service, &tab);
    }

    if (tab_groups::SavedTabGroupUtils::SupportsSharedTabGroups()) {
      collaboration_messaging_tab_data_ =
          GetUserDataFactory()
              .CreateInstance<tab_groups::CollaborationMessagingTabData>(tab,
                                                                         &tab);
    }

    if (tab_groups::SavedTabGroupUtils::SupportsSharedTabGroups() &&
        page_action_controller_->ActionExists(
            kActionShowCollaborationRecentActivity)) {
      collaboration_messaging_page_action_controller_ =
          GetUserDataFactory()
              .CreateInstance<CollaborationMessagingPageActionController>(
                  tab, tab, *page_action_controller_,
                  *collaboration_messaging_tab_data_);
    }

    if (base::FeatureList::IsEnabled(commerce::kInStockNotification) &&
        !profile->IsPrimaryOTRProfileWithRegularParent()) {
      in_stock_notification_manager_ =
          GetUserDataFactory()
              .CreateInstance<commerce::InStockNotificationManager>(tab, &tab);
    }

    if (glic::GlicEnabling::IsProfileEligible(profile)) {
      glic_instance_helper_ =
          GetUserDataFactory().CreateInstance<glic::GlicInstanceHelper>(tab,
                                                                        &tab);
      glic_tab_indicator_helper_ =
          GetUserDataFactory().CreateInstance<glic::GlicTabIndicatorHelper>(
              tab, &tab);
      selection_suggestion_service_ =
          GetUserDataFactory().CreateInstance<selection::SuggestionService>(
              tab, &tab,
              OptimizationGuideKeyedServiceFactory::GetForProfile(profile),
              GoogleGroupsManagerFactory::GetForBrowserContext(profile));
      if (base::FeatureList::IsEnabled(features::kGlicSelectionSuggestions)) {
        glic_selection_suggestion_tool_ =
            std::make_unique<glic::SelectionSuggestionTool>(tab);
      }
      glic_selection_overlay_controller_ =
          GetUserDataFactory().CreateInstance<glic::SelectionOverlayController>(
              tab, &tab, profile->GetPrefs());
      glic_promotion_source_navigation_observer_ =
          std::make_unique<glic::GlicPromotionSourceNavigationObserver>(&tab);

      if (glic::GlicEnabling::IsEnabledForProfile(profile)) {
        glic_selection_observer_ =
            std::make_unique<glic::GlicSelectionObserver>(tab.GetContents());
      }
      if (base::FeatureList::IsEnabled(
              features::kGlicSummarizeVideoSuggestion)) {
        glic_page_features_manager_ =
            GetUserDataFactory().CreateInstance<glic::GlicPageFeaturesManager>(
                tab, &tab);
      }
    }
    if (glic::GlicKeyedService::Get(profile)) {
      glic_side_panel_coordinator_ =
          GetUserDataFactory()
              .CreateInstance<glic::GlicSidePanelCoordinatorImpl>(
                  tab, &tab, side_panel_registry_.get());
    }
    // TODO(crbug.com/433973411): Move this logic to a helper function.
    if (base::FeatureList::IsEnabled(features::kGlicActor) &&
        base::FeatureList::IsEnabled(features::kGlicActorUi) &&
        profile->IsRegularProfile()) {
      // The associated tab is passed to CreateInstance twice: for dependency
      // injection callbacks and as a direct constructor argument.
      actor_ui_tab_controller_ =
          GetUserDataFactory().CreateInstance<actor::ui::ActorUiTabController>(
              tab, tab, actor::ActorKeyedService::Get(profile));
    }
    if (base::FeatureList::IsEnabled(features::kSkillsEnabled)) {
      skills_ui_tab_controller_ =
          GetUserDataFactory().CreateInstance<skills::SkillsUiTabController>(
              tab, tab);
    }
  }  // IsInNormalWindow() end.

  if (base::FeatureList::IsEnabled(features::kGlicActor)) {
    actor_tab_data_ =
        GetUserDataFactory().CreateInstance<actor::ActorTabData>(tab, &tab);
    actor_surface_tab_helper_ =
        GetUserDataFactory().CreateInstance<actor::ActorSurfaceTabHelper>(tab,
                                                                          tab);
  }

  // This block instantiates the page action controllers that depends on the
  // `commerce_ui_tab_helper_` and not need to be created before.
  if (commerce_ui_tab_helper_ &&
      page_action_controller_->ActionExists(kActionCommerceDiscounts)) {
    commerce_discounts_page_action_view_controller_ =
        GetUserDataFactory()
            .CreateInstance<commerce::DiscountsPageActionViewController>(
                tab, tab, *page_action_controller_, *commerce_ui_tab_helper_);
  }

  autofill_bubble_manager_ = autofill::BubbleManager::Create(&tab);

  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableOmniboxAutofill) &&
      page_action_controller_->ActionExists(kActionAutofillPayment)) {
    omnibox_autofill_page_action_controller_ =
        std::make_unique<autofill::OmniboxAutofillPageActionController>(
            tab, *page_action_controller_);
    omnibox_autofill_bubble_controller_ =
        GetUserDataFactory()
            .CreateInstance<autofill::OmniboxAutofillBubbleController>(
                tab, tab, tab.GetContents());
  }

  if (page_action_controller_->ActionExists(
          kActionShowPaymentsChurnedUsersBubble)) {
    payments_churned_users_page_action_controller_ =
        std::make_unique<autofill::PaymentsChurnedUsersPageActionController>(
            tab, *page_action_controller_);
    payments_churned_users_bubble_controller_ =
        GetUserDataFactory()
            .CreateInstance<autofill::PaymentsChurnedUsersBubbleController>(
                tab, tab, tab.GetContents());
  }

  if (page_action_controller_->ActionExists(kActionWalletReminderNotice)) {
    wallet_reminder_notice_page_action_controller_ =
        GetUserDataFactory()
            .CreateInstance<autofill::WalletReminderNoticePageActionController>(
                tab, tab, *page_action_controller_);
    wallet_reminder_notice_bubble_controller_ =
        GetUserDataFactory()
            .CreateInstance<autofill::WalletReminderNoticeBubbleController>(
                tab, tab, tab.GetContents());
  }

  gmail_otp_opt_in_bubble_controller_ =
      GetUserDataFactory()
          .CreateInstance<autofill::GmailOtpOptInBubbleController>(tab, tab);

  customize_chrome_side_panel_controller_ =
      std::make_unique<customize_chrome::SidePanelControllerViews>(tab);

  extension_side_panel_manager_ =
      std::make_unique<extensions::ExtensionSidePanelManager>(
          profile, &tab, side_panel_registry_.get());

  tab_dialog_manager_ = std::make_unique<TabDialogManager>(&tab);

  data_protection_tab_controller_ = std::make_unique<
      enterprise_data_protection::DataProtectionNavigationController>(&tab);

  enterprise_proxy_tab_helper_ =
      GetUserDataFactory()
          .CreateInstance<enterprise_net::EnterpriseProxyTabHelper>(
              tab, tab, tab.GetContents(),
              EnterpriseProxyErrorServiceFactory::GetForProfile(profile));

  // Create the ReadAnythingController first to ensure it exists before
  // any potential consumers, like the side panel controller.
  read_anything_controller_ =
      GetUserDataFactory().CreateInstance<ReadAnythingController>(
          tab, &tab, side_panel_registry_.get());

  // Create the HttpAuthCacheStatus to start observing resource load
  // completions.
  http_auth_cache_status_ =
      std::make_unique<HttpAuthCacheStatus>(tab.GetContents());

  if (web_app::AreWebAppsEnabled(profile)) {
    web_app::WebAppTabHelper::Create(&tab, tab.GetContents());
  }

  security_state_event_observer_ =
      std::make_unique<SecurityStateEventObserver>(tab.GetContents());

  sync_sessions_router_ =
      std::make_unique<sync_sessions::SyncSessionsRouterTabHelper>(
          tab.GetContents(),
          sync_sessions::SyncSessionsWebContentsRouterFactory::GetForProfile(
              profile),
          ChromeTranslateClient::FromWebContents(tab.GetContents()),
          favicon::ContentFaviconDriver::FromWebContents(tab.GetContents()));

  browser_synced_tab_delegate_ =
      GetUserDataFactory().CreateInstance<BrowserSyncedTabDelegate>(
          tab, tab, tab.GetContents());

  focus_tab_after_navigation_helper_ =
      std::make_unique<FocusTabAfterNavigationHelper>(tab.GetContents());

  framebust_block_tab_helper_ =
      GetUserDataFactory().CreateInstance<FramebustBlockTabHelper>(
          tab, tab, tab.GetContents());

  connection_help_tab_helper_ =
      GetUserDataFactory().CreateInstance<ConnectionHelpTabHelper>(
          tab, tab, tab.GetContents());

  form_interaction_tab_helper_ =
      GetUserDataFactory().CreateInstance<FormInteractionTabHelper>(tab, tab);

  zero_suggest_prefetch_tab_helper_ =
      std::make_unique<ZeroSuggestPrefetchTabHelper>(tab.GetContents());

  if (SearchEngineChoiceTabHelper::IsHelperNeeded()) {
    search_engine_choice_tab_helper_ =
        std::make_unique<SearchEngineChoiceTabHelper>(tab.GetContents());
  }

  intent_picker_tab_helper_ =
      std::make_unique<IntentPickerTabHelper>(tab, tab.GetContents());

  if (base::FeatureList::IsEnabled(features::kTabHoverCardImages)) {
    thumbnail_tab_helper_ =
        GetUserDataFactory().CreateInstance<ThumbnailTabHelper>(
            tab, tab, tab.GetContents());
  }

  if (!webui_browser::IsWebUIBrowserEnabled()) {
    sad_tab_helper_ = GetUserDataFactory().CreateInstance<SadTabHelper>(
        tab, tab, tab.GetContents());
  }

  from_gws_navigation_and_keep_alive_request_observer_ =
      FromGWSNavigationAndKeepAliveRequestObserver::MaybeCreate(
          tab.GetContents());

  resource_usage_helper_ =
      GetUserDataFactory().CreateInstance<TabResourceUsageTabHelper>(tab, tab);

  memory_saver_chip_helper_ = std::make_unique<MemorySaverChipTabHelper>(tab);

  tab_creation_metrics_controller_ =
      std::make_unique<TabCreationMetricsController>(&tab);

  tab_attachment_tracker_ =
      GetUserDataFactory().CreateInstance<TabAttachmentTracker>(tab, &tab);

  tab_ui_helper_ = GetUserDataFactory().CreateInstance<TabUIHelper>(tab, tab);

  task_manager::WebContentsTags::CreateForTabContents(tab.GetContents());

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  desktop_session_duration_observer_ =
      metrics::DesktopSessionDurationObserver::MaybeCreate(tab.GetContents());
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  inactive_window_mouse_event_controller_ =
      std::make_unique<InactiveWindowMouseEventController>();

  if (base::FeatureList::IsEnabled(
          wallet::features::kWalletablePassDetection)) {
    walletable_pass_client_ =
        std::make_unique<wallet::ChromeWalletablePassClient>(&tab);
  }

  if (base::FeatureList::IsEnabled(contextual_tasks::kContextualTasksContext)) {
    contextual_tasks_tab_visit_tracker_ =
        GetUserDataFactory()
            .CreateInstance<contextual_tasks::ContextualTasksTabVisitTracker>(
                tab, tab);
  }

  if (contextual_tasks::IsCopyTextJourneysEnabled()) {
    copy_search_journey_tab_feature_ =
        GetUserDataFactory()
            .CreateInstance<contextual_tasks::CopySearchJourneyTabFeature>(tab,
                                                                           tab);
  }
#endif

#if BUILDFLAG(IS_WIN)
  if (base::FeatureList::IsEnabled(
          feature_engagement::kIPHSearchPromotionFeature)) {
    search_promotion_navigation_observer_ =
        GetUserDataFactory().CreateInstance<SearchPromotionNavigationObserver>(
            tab, tab);
  }
  commit_limit_oom_recovery_tracker_ =
      GetUserDataFactory().CreateInstance<CommitLimitOOMRecoveryTracker>(tab,
                                                                         tab);
  font_prewarmer_tab_helper_ =
      std::make_unique<FontPrewarmerTabHelper>(tab.GetContents());
#endif

  if (base::FeatureList::IsEnabled(net::features::kVerifyQWACs)) {
    qwac_web_contents_observer_ =
        std::make_unique<QwacWebContentsObserver>(tab);
  }

  if (base::FeatureList::IsEnabled(contextual_cueing::kContextualCueingV2)) {
    contextual_cueing_controller_ =
        std::make_unique<contextual_cueing::ContextualCueingController>(&tab);
    glic::GlicCueTarget::Register(tab);
    contextual_search::ContextualSearchCueTarget::Register(tab);
  }

  if (auto* contextual_cueing_service =
          contextual_cueing::ContextualCueingServiceFactory::GetForProfile(
              profile)) {
    contextual_cueing_web_contents_observer_ = std::make_unique<
        contextual_cueing::ContextualCueingWebContentsObserver>(
        tab.GetContents(), contextual_cueing_service);
  }

  if (base::FeatureList::IsEnabled(
          security_interstitials::features::kHttpsFirstDialogUi)) {
    ask_before_http_dialog_controller_ =
        GetUserDataFactory().CreateInstance<AskBeforeHttpDialogController>(
            tab, &tab);
  }

  bookmarkbar_preload_pipeline_manager_ =
      std::make_unique<BookmarkBarPreloadPipelineManager>(tab.GetContents());

  context_highlight_tab_feature_ =
      GetUserDataFactory().CreateInstance<ContextHighlightTabFeature>(tab, tab);

  new_tab_page_preload_pipeline_manager_ =
      std::make_unique<NewTabPagePreloadPipelineManager>(tab.GetContents());

  vr_tab_helper_ =
      GetUserDataFactory().CreateInstance<vr::VrTabHelper>(tab, tab);

  recently_audible_helper_ =
      GetUserDataFactory().CreateInstance<RecentlyAudibleHelper>(
          tab, tab, tab.GetContents());

  child_tab_alert_helper_ =
      GetUserDataFactory().CreateInstance<ChildTabAlertHelper>(tab, tab);

  tab_alert_controller_ =
      GetUserDataFactory().CreateInstance<TabAlertController>(tab, tab);

  if (base::FeatureList::IsEnabled(
          record_replay::features::kRecordReplayBase)) {
    record_replay_client_ =
        GetUserDataFactory().CreateInstance<ChromeRecordReplayClient>(tab, tab);
  }

  tab_contextualization_controller_ =
      GetUserDataFactory().CreateInstance<lens::TabContextualizationController>(
          tab, &tab);

#if BUILDFLAG(IS_CHROMEOS)
  if (apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    protocol_handler_picker_coordinator_ =
        GetUserDataFactory()
            .CreateInstance<web_app::ProtocolHandlerPickerCoordinator>(
                tab, tab, apps::AppServiceProxyFactory::GetForProfile(profile));
  }
  google_one_offer_iph_tab_helper_ =
      std::make_unique<GoogleOneOfferIphTabHelper>(tab.GetContents());
  // Do not create for Incognito mode.
  if (!profile->IsOffTheRecord()) {
    campaigns_manager_session_tab_helper_ =
        std::make_unique<CampaignsManagerSessionTabHelper>(tab.GetContents());
  }
  cros_isolated_web_app_enabler_ =
      std::make_unique<ash::CrosIsolatedWebAppEnabler>(tab.GetContents());
  gemini_app_tab_helper_ = GeminiAppTabHelper::MaybeCreate(tab.GetContents());
  mahi_tab_helper_ = mahi::MahiTabHelper::MaybeCreate(tab.GetContents());
  web_time_navigation_observer_ =
      ash::app_time::WebTimeNavigationObserver::MaybeCreate(tab,
                                                            tab.GetContents());
  boot_times_recorder_tab_helper_ =
      ash::BootTimesRecorderTabHelper::MaybeCreate(tab.GetContents());
  policy::DlpContentTabHelper::MaybeCreateForWebContents(tab.GetContents());
#endif

  // The controller is created for all tabs but only affects back button
  // behavior for destination tabs with opener relationships.
  if (base::FeatureList::IsEnabled(tabs::kBackToOpener)) {
    back_to_opener_controller_ =
        std::make_unique<back_to_opener::BackToOpenerController>(tab);
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(enterprise_reporting::kSaasUsageReporting)) {
    saas_usage_navigation_observer_ =
        std::make_unique<enterprise_reporting::SaasUsageNavigationObserver>(
            tab.GetContents());
  }
  if (base::FeatureList::IsEnabled(
          features::kHappinessTrackingSurveysForDesktopDemo) ||
      base::FeatureList::IsEnabled(features::kTrustSafetySentimentSurvey) ||
      base::FeatureList::IsEnabled(features::kTrustSafetySentimentSurveyV2) ||
      PerformanceControlsHatsServiceFactory::IsAnySurveyFeatureEnabled()) {
    hats_helper_ = std::make_unique<HatsHelper>(tab.GetContents());
  }
  shared_highlighting_promo_ =
      std::make_unique<SharedHighlightingPromo>(tab.GetContents());
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(multistep_filter::kMultistepFilter)) {
    filter_ui_controller_ =
        GetUserDataFactory()
            .CreateInstance<multistep_filter::FilterUiController>(tab, tab);
    filter_navigation_observer_ =
        GetUserDataFactory()
            .CreateInstance<multistep_filter::ChromeFilterNavigationObserver>(
                tab, tab);
  }
#endif

  if (base::FeatureList::IsEnabled(features::kSkillsEnabled)) {
    skills_update_observer_ =
        std::make_unique<skills::SkillsUpdateObserver>(tab);
  }
  if (base::FeatureList::IsEnabled(features::kIndigo)) {
    indigo_page_action_controller_ =
        std::make_unique<indigo::IndigoPageActionController>(
            tab, *page_action_controller_);
    if (base::FeatureList::IsEnabled(contextual_cueing::kContextualCueingV2) &&
        base::FeatureList::IsEnabled(features::kIndigoContextualCueingV2)) {
      indigo::IndigoCueTarget::Register(tab);
    }
  }

  if (base::FeatureList::IsEnabled(
          payments::features::kThreeDSecureTelemetry)) {
    web_payments_observer_ =
        std::make_unique<payments::WebPaymentsObserver>(tab.GetContents());
  }

  uma_browsing_activity_tab_helper_ =
      std::make_unique<UMABrowsingActivityTabHelper>(tab.GetContents());

  window_management_content_setting_observer_ =
      std::make_unique<web_app::WindowManagementContentSettingObserver>(
          tab.GetContents());

#if BUILDFLAG(ENABLE_RLZ)
  chrome_rlz_tracker_web_contents_observer_ =
      ChromeRLZTrackerWebContentsObserver::MaybeCreate(tab.GetContents());
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
  plugin_observer_ = GetUserDataFactory().CreateInstance<PluginObserver>(
      tab, tab, tab.GetContents());
#endif

  tab_capture_contents_border_helper_ =
      GetUserDataFactory().CreateInstance<TabCaptureContentsBorderHelper>(tab,
                                                                          tab);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (base::FeatureList::IsEnabled(switches::kEnableSearchAIModeSigninPromo) &&
      base::FeatureList::IsEnabled(contextual_tasks::kContextualTasks)) {
    search_ai_mode_promo_tab_helper_ =
        GetUserDataFactory()
            .CreateInstance<contextual_tasks::SearchAiModePromoTabHelper>(
                tab, tab, tab.GetContents());
  }
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  captive_portal::CaptivePortalTabHelper::CreateForWebContents(
      tab.GetContents(), CaptivePortalServiceFactory::GetForProfile(profile),
      base::BindRepeating(
          &ChromeSecurityBlockingPageFactory::OpenLoginTabForWebContents,
          tab.GetContents(), false));
#endif

#if BUILDFLAG(ENABLE_COMPOSE)
  // We need to create the ChromeComposeClient to listen for the feature
  // being turned on, even if it is not enabled yet.
  // FieldChangeObserver in ChromeComposeClient uses
  // ScopedAutofillManagersObservation which expects ContentAutofillClient.
  if (!profile->IsOffTheRecord() &&
      autofill::ContentAutofillClient::FromWebContents(tab.GetContents())) {
    compose_client_ = GetUserDataFactory().CreateInstance<ChromeComposeClient>(
        tab, tab, tab.GetContents());
  }
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  app_tab_helper_ =
      std::make_unique<extensions::AppTabHelper>(tab, tab.GetContents());
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // If the web contents already have a view type, don't overwrite it here. One
  // case where this can happen is when the user opens undocked developer tools.
  // For all developer tools web contents, the view type is set to
  // `kDeveloperTools` by the `DevToolsWindow` before tab helpers are attached.
  if (extensions::GetViewType(tab.GetContents()) ==
      extensions::mojom::ViewType::kInvalid) {
    extensions::SetViewType(tab.GetContents(),
                            extensions::mojom::ViewType::kTabContents);
  }
  extensions::WebNavigationTabObserver::CreateForWebContents(tab.GetContents());
  extensions::TabHelper::CreateForWebContents(tab.GetContents());
  navigation_extension_enabler_ =
      std::make_unique<extensions::NavigationExtensionEnabler>(
          tab.GetContents());
#endif

  if (base::FeatureList::IsEnabled(features::kGlicMarketingAutoOpen)) {
    glic_marketing_page_tab_helper_ =
        std::make_unique<glic::GlicMarketingPageTabHelper>(tab.GetContents());
  }

  tab_context_decryption_token_tab_helper_ =
      TabContextDecryptionTokenTabHelper::MaybeCreate(tab.GetContents());

  v8_compile_hints_tab_helper_ =
      v8_compile_hints::V8CompileHintsTabHelper::MaybeCreate(tab.GetContents());

  storage_access_api_tab_helper_ = std::make_unique<StorageAccessAPITabHelper>(
      tab.GetContents(),
      StorageAccessAPIServiceFactory::GetForBrowserContext(profile));

  if (auto* service =
          RevokedPermissionsServiceFactory::GetForProfile(profile)) {
    revoked_permissions_tab_helper_ =
        std::make_unique<RevokedPermissionsTabHelper>(tab.GetContents(),
                                                      service);
  }

  external_protocol_observer_ =
      std::make_unique<ExternalProtocolObserver>(tab.GetContents());

  no_state_prefetch_tab_helper_ =
      std::make_unique<prerender::NoStatePrefetchTabHelper>(tab.GetContents());

  navigation_predictor_preconnect_client_ =
      std::make_unique<NavigationPredictorPreconnectClient>(tab.GetContents());

  navigation_metrics_recorder_ =
      std::make_unique<NavigationMetricsRecorder>(tab.GetContents());

  site_protection_metrics_observer_ =
      std::make_unique<site_protection::SiteProtectionMetricsObserver>(
          tab.GetContents());

  permissions::PermissionRequestManager::CreateForWebContents(
      tab.GetContents());

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  safe_browsing::SafeBrowsingNavigationObserver::MaybeCreateForWebContents(
      tab.GetContents(), HostContentSettingsMapFactory::GetForProfile(profile),
      safe_browsing::SafeBrowsingNavigationObserverManagerFactory::
          GetForBrowserContext(profile),
      profile->GetPrefs(), g_browser_process->safe_browsing_service(),
      enterprise_connectors::IsReferrerChainNeededForEnterprise(profile));
  if (autofill::ContentAutofillClient::FromWebContents(tab.GetContents())) {
    ChromePasswordReuseDetectionManagerClient::CreateForWebContents(
        tab.GetContents());
    safe_browsing_tab_observer_ =
        GetUserDataFactory()
            .CreateInstance<safe_browsing::SafeBrowsingTabObserver>(
                tab, tab, tab.GetContents(),
                std::make_unique<
                    safe_browsing::ChromeSafeBrowsingTabObserverDelegate>());
  }
  if (base::FeatureList::IsEnabled(
          safe_browsing::kTailoredSecurityIntegration)) {
    tailored_security_url_observer_ =
        std::make_unique<safe_browsing::TailoredSecurityUrlObserver>(
            tab.GetContents(),
            safe_browsing::TailoredSecurityServiceFactory::GetForProfile(
                profile));
  }
  trigger_creator_ = std::make_unique<safe_browsing::TriggerCreator>(
      tab, profile, tab.GetContents());
#endif

  if (page_info::IsAboutThisSiteFeatureEnabled()) {
    if (auto* optimization_guide_decider =
            OptimizationGuideKeyedServiceFactory::GetForProfile(profile)) {
      about_this_site_tab_helper_ =
          GetUserDataFactory().CreateInstance<AboutThisSiteTabHelper>(
              tab, tab, tab.GetContents(), optimization_guide_decider);
    }
  }

  sound_content_setting_observer_ =
      GetUserDataFactory().CreateInstance<SoundContentSettingObserver>(
          tab, tab, tab.GetContents());

  task_tab_helper_ = GetUserDataFactory().CreateInstance<tasks::TaskTabHelper>(
      tab, tab, tab.GetContents());

  if (!profile->IsOffTheRecord() &&
      HistoryEmbeddingsServiceFactory::GetForProfile(profile)) {
    history_embeddings_tab_helper_ =
        std::make_unique<HistoryEmbeddingsTabHelper>(tab.GetContents());
  }

  download_navigation_observer_ =
      std::make_unique<download::DownloadNavigationObserver>(
          tab.GetContents(), download::NavigationMonitorFactory::GetForKey(
                                 profile->GetProfileKey()));

  web_contents_top_sites_observer_ =
      std::make_unique<history::WebContentsTopSitesObserver>(
          tab.GetContents(), TopSitesFactory::GetForProfile(profile).get());

  client_hints_web_contents_observer_ =
      std::make_unique<client_hints::ClientHintsWebContentsObserver>(
          tab.GetContents());

  chained_back_navigation_tracker_ =
      GetUserDataFactory().CreateInstance<ChainedBackNavigationTracker>(
          tab, tab, tab.GetContents());

  file_system_access_tab_helper_ =
      std::make_unique<FileSystemAccessTabHelper>(tab.GetContents());

  search_engine_tab_helper_ =
      GetUserDataFactory().CreateInstance<SearchEngineTabHelper>(
          tab, tab, tab.GetContents());

  net_error_tab_helper_ =
      GetUserDataFactory()
          .CreateInstance<chrome_browser_net::NetErrorTabHelper>(
              tab, tab, tab.GetContents());

#if BUILDFLAG(IS_CHROMEOS)
  // Do not create for Incognito and Isolated mode.
  if (!profile->IsPrimaryOTRProfileWithRegularParent()) {
    supervised_user_navigation_observer_ =
        GetUserDataFactory().CreateInstance<SupervisedUserNavigationObserver>(
            tab, tab, tab.GetContents());
  }
#else
  // Do not create for OTR.
  if (!profile->IsOffTheRecord()) {
    supervised_user_navigation_observer_ =
        GetUserDataFactory().CreateInstance<SupervisedUserNavigationObserver>(
            tab, tab, tab.GetContents());
  }
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  offline_page_tab_helper_ =
      GetUserDataFactory().CreateInstance<offline_pages::OfflinePageTabHelper>(
          tab, tab, tab.GetContents());
  recent_tab_helper_ =
      GetUserDataFactory().CreateInstance<offline_pages::RecentTabHelper>(
          tab, tab, tab.GetContents());
#endif

  webapps::PreRedirectionURLObserver::CreateForWebContents(tab.GetContents());

  if (search::IsInstantExtendedAPIEnabled()) {
    search_tab_helper_ = GetUserDataFactory().CreateInstance<SearchTabHelper>(
        tab, tab, tab.GetContents());
  }

  auto_picture_in_picture_tab_helper_ =
      GetUserDataFactory().CreateInstance<AutoPictureInPictureTabHelper>(
          tab, tab, tab.GetContents());

  manage_passwords_ui_controller_ =
      GetUserDataFactory().CreateInstance<ManagePasswordsUIController>(
          tab, tab, tab.GetContents());

  auto* optimization_guide_decider =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (autofill::ContentAutofillClient::FromWebContents(tab.GetContents()) &&
      optimization_guide_decider &&
      base::FeatureList::IsEnabled(
          payments::facilitated::kEnableDesktopQrCodeDetection)) {
    chrome_facilitated_payments_client_ =
        GetUserDataFactory().CreateInstance<ChromeFacilitatedPaymentsClient>(
            tab, tab, tab.GetContents(), optimization_guide_decider);
  }

  if (auto* metrics_services_manager =
          g_browser_process->GetMetricsServicesManager()) {
    metrics_services_web_contents_observer_ =
        std::make_unique<metrics::MetricsServicesWebContentsObserver>(
            tab.GetContents(),
            metrics_services_manager->GetOnDidStartLoadingCb(),
            metrics_services_manager->GetOnDidStopLoadingCb(),
            metrics_services_manager->GetOnRendererUnresponsiveCb());
  }

  popup_opener_tab_helper_ =
      GetUserDataFactory()
          .CreateInstance<blocked_content::PopupOpenerTabHelper>(
              tab, tab, tab.GetContents(),
              base::DefaultTickClock::GetInstance(),
              HostContentSettingsMapFactory::GetForProfile(profile));

  if (auto* page_content_annotations_service =
          PageContentAnnotationsServiceFactory::GetForProfile(profile)) {
    page_content_annotations_web_contents_observer_ =
        GetUserDataFactory()
            .CreateInstance<page_content_annotations::
                                PageContentAnnotationsWebContentsObserver>(
                tab, tab, tab.GetContents(), *page_content_annotations_service);
  }

  core_tab_helper_ = GetUserDataFactory().CreateInstance<CoreTabHelper>(
      tab, tab, tab.GetContents());

  FindBarState::ConfigureWebContents(tab.GetContents());

  if (MediaEngagementService::IsEnabled()) {
    MediaEngagementService::CreateWebContentsObserver(tab.GetContents());
  }

#if BUILDFLAG(ENABLE_PRINTING)
  printing::InitializePrintingForWebContents(tab.GetContents());
#endif

  mixed_content_settings_tab_helper_ =
      GetUserDataFactory().CreateInstance<MixedContentSettingsTabHelper>(
          tab, tab, tab.GetContents());

  permissions::PermissionRecoverySuccessRateTracker::CreateForWebContents(
      tab.GetContents());

  javascript_dialogs::TabModalDialogManager::CreateForWebContents(
      tab.GetContents(),
      std::make_unique<JavaScriptTabModalDialogManagerDelegateDesktop>(
          tab.GetContents()));

  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      tab.GetContents());

  // Attach TrustedVaultEncryptionKeysTabHelper to the tab.
  TrustedVaultEncryptionKeysTabHelper::CreateForWebContents(tab.GetContents());

  // Track one-time permissions for the tab.
  one_time_permissions_tracker_helper_ =
      std::make_unique<OneTimePermissionsTrackerHelper>(tab.GetContents());

  if (predictors::LoadingPredictorFactory::GetForProfile(profile)) {
    loading_predictor_tab_helper_ =
        GetUserDataFactory()
            .CreateInstance<predictors::LoadingPredictorTabHelper>(
                tab, tab, tab.GetContents());
  }

  if (breadcrumbs::IsEnabled(g_browser_process->local_state())) {
    BreadcrumbManagerTabHelper::CreateForWebContents(tab.GetContents());
  }

  PrefsTabHelper::CreateForWebContents(tab.GetContents());

  if (site_engagement::SiteEngagementService::IsEnabled()) {
    site_engagement::SiteEngagementService::Helper::CreateForWebContents(
        tab.GetContents(),
        prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(
            profile));
  }

  SafetyTipWebContentsObserver::CreateForWebContents(tab.GetContents());

  HttpsOnlyModeTabHelper::CreateForWebContents(tab.GetContents());

  login_detection::LoginDetectionTabHelper::MaybeCreateForWebContents(
      tab.GetContents());

  if (!profile->IsOffTheRecord()) {
    HistoryClustersTabHelper::CreateForWebContents(
        tab.GetContents(),
        HistoryTabHelper::FromWebContents(tab.GetContents()));
  }

  blocked_content::PopupBlockerTabHelper::CreateForWebContents(
      tab.GetContents());

  resource_coordinator::ResourceCoordinatorTabHelper::CreateForWebContents(
      tab.GetContents());

  ukm::InitializeSourceUrlRecorderForWebContents(tab.GetContents());
}

TabUIHelper* TabFeatures::SetTabUIHelperForTesting(
    std::unique_ptr<TabUIHelper> tab_ui_helper) {
  tab_ui_helper_ = std::move(tab_ui_helper);
  return tab_ui_helper_.get();
}

lens::TabContextualizationController*
TabFeatures::SetTabContextualizationControllerForTesting(
    std::unique_ptr<lens::TabContextualizationController>
        tab_contextualization_controller) {
  tab_contextualization_controller_ =
      std::move(tab_contextualization_controller);
  return tab_contextualization_controller_.get();
}

autofill::BubbleManager* TabFeatures::SetBubbleManagerForTesting(
    std::unique_ptr<autofill::BubbleManager> bubble_manager) {
  autofill_bubble_manager_ = std::move(bubble_manager);
  return autofill_bubble_manager_.get();
}

void TabFeatures::WillDiscardContents(tabs::TabInterface* tab,
                                      content::WebContents* old_contents,
                                      content::WebContents* new_contents) {
  DCHECK_EQ(old_contents, tab->GetContents());

  Profile* profile = tab->GetBrowserWindowInterface()->GetProfile();

  // Deregister side-panel entries that are web-contents scoped rather than tab
  // scoped.
  side_panel_registry_->Deregister(
      SidePanelEntry::Key(SidePanelEntry::Id::kAboutThisSite));
  side_panel_registry_->Deregister(
      SidePanelEntry::Key(SidePanelEntry::Id::kMerchantTrust));

  if (web_app::AreWebAppsEnabled(
          tab->GetBrowserWindowInterface()->GetProfile())) {
    web_app::WebAppTabHelper::Create(tab, new_contents);
  }

  BookmarkTabHelper::CreateForWebContents(new_contents);

  focus_tab_after_navigation_helper_ =
      std::make_unique<FocusTabAfterNavigationHelper>(new_contents);

  // The reset() must happen first so that the old instance deregisters
  // itself from the UnownedUserDataHost before the new instance registers
  // itself.
  framebust_block_tab_helper_.reset();
  framebust_block_tab_helper_ =
      GetUserDataFactory().CreateInstance<FramebustBlockTabHelper>(
          *tab, *tab, new_contents);

  // The reset() must happen first so that the old instance deregisters
  // itself from the UnownedUserDataHost before the new instance registers
  // itself.
  connection_help_tab_helper_.reset();
  connection_help_tab_helper_ =
      GetUserDataFactory().CreateInstance<ConnectionHelpTabHelper>(
          *tab, *tab, new_contents);

  // Recreated to reset its state: the swapped-in contents has not had any
  // form interactions. The reset() must happen first so that the old
  // instance deregisters itself from the UnownedUserDataHost before the new
  // instance registers itself.
  form_interaction_tab_helper_.reset();
  form_interaction_tab_helper_ =
      GetUserDataFactory().CreateInstance<FormInteractionTabHelper>(*tab, *tab);

  if (ml_installability_promoter_) {
    ml_installability_promoter_.reset();
    ml_installability_promoter_ =
        GetUserDataFactory().CreateInstance<webapps::MLInstallabilityPromoter>(
            *tab, *tab, new_contents);
  }

  if (app_banner_manager_) {
    // Observers of the old manager (e.g. PwaInstallPageAction, the
    // autotestPrivate waiter) detach in their own WillDiscardContents
    // callbacks. Those run after this one: callbacks fire in registration
    // order, and TabFeatures — the owner performing the swap — necessarily
    // registers before anything it creates in Init(), while some observers
    // register at arbitrary later times. Deregister the old manager from the
    // tab now so the replacement can register (and later callbacks in this
    // pass resolve the new instance), but destroy it asynchronously so it
    // outlives every detach callback regardless of registration order.
    // TODO(crbug.com/347770670): once tab discarding in its current
    // contents-swapping form goes away, the deferred destruction (and
    // DeregisterFromTabForDiscard) can be removed.
    app_banner_manager_->DeregisterFromTabForDiscard();
    base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(app_banner_manager_));
    app_banner_manager_ =
        GetUserDataFactory()
            .CreateInstanceWithFactoryMethod<webapps::AppBannerManagerDesktop,
                                             tabs::TabInterface&,
                                             content::WebContents*>(
                *tab, &webapps::AppBannerManagerDesktop::Create, *tab,
                new_contents);
  }

  zero_suggest_prefetch_tab_helper_ =
      std::make_unique<ZeroSuggestPrefetchTabHelper>(new_contents);

  security_state_event_observer_ =
      std::make_unique<SecurityStateEventObserver>(new_contents);

  enterprise_proxy_tab_helper_.reset();
  enterprise_proxy_tab_helper_ =
      GetUserDataFactory()
          .CreateInstance<enterprise_net::EnterpriseProxyTabHelper>(
              *tab, *tab, new_contents,
              EnterpriseProxyErrorServiceFactory::GetForProfile(profile));

  http_auth_cache_status_ = std::make_unique<HttpAuthCacheStatus>(new_contents);

  if (search_engine_choice_tab_helper_) {
    search_engine_choice_tab_helper_ =
        std::make_unique<SearchEngineChoiceTabHelper>(new_contents);
  }

  // The reset() must happen first so that the old instance deregisters
  // itself from the UnownedUserDataHost before the new instance registers
  // itself.
  intent_picker_tab_helper_.reset();
  intent_picker_tab_helper_ =
      std::make_unique<IntentPickerTabHelper>(*tab, new_contents);

  if (thumbnail_tab_helper_) {
    // The old helper stashed its thumbnail data on `new_contents` from
    // AboutToBeDiscarded(); the new helper picks it up in its constructor.
    // The reset() must happen first so that the old instance deregisters
    // itself from the UnownedUserDataHost before the new instance registers
    // itself.
    thumbnail_tab_helper_.reset();
    thumbnail_tab_helper_ =
        GetUserDataFactory().CreateInstance<ThumbnailTabHelper>(*tab, *tab,
                                                                new_contents);
  }

  if (sad_tab_helper_) {
    // The reset() must happen first so that the old instance deregisters
    // itself from the UnownedUserDataHost before the new instance registers
    // itself.
    sad_tab_helper_.reset();
    sad_tab_helper_ = GetUserDataFactory().CreateInstance<SadTabHelper>(
        *tab, *tab, new_contents);
  }

  from_gws_navigation_and_keep_alive_request_observer_ =
      FromGWSNavigationAndKeepAliveRequestObserver::MaybeCreate(new_contents);

  if (auto* contextual_cueing_service =
          contextual_cueing::ContextualCueingServiceFactory::GetForProfile(
              profile)) {
    contextual_cueing_web_contents_observer_ = std::make_unique<
        contextual_cueing::ContextualCueingWebContentsObserver>(
        new_contents, contextual_cueing_service);
  }

  sync_sessions_router_.reset();
  sync_sessions_router_ =
      std::make_unique<sync_sessions::SyncSessionsRouterTabHelper>(
          new_contents,
          sync_sessions::SyncSessionsWebContentsRouterFactory::GetForProfile(
              profile),
          ChromeTranslateClient::FromWebContents(new_contents),
          favicon::ContentFaviconDriver::FromWebContents(new_contents));

  // The reset() must happen first so that the old instance deregisters
  // itself from the UnownedUserDataHost before the new instance registers
  // itself.
  browser_synced_tab_delegate_.reset();
  browser_synced_tab_delegate_ =
      GetUserDataFactory().CreateInstance<BrowserSyncedTabDelegate>(
          *tab, *tab, new_contents);

  if (permission_indicators_tab_data_) {
    permission_indicators_tab_data_ =
        std::make_unique<permissions::PermissionIndicatorsTabData>(
            new_contents);
  }

  if (bookmarkbar_preload_pipeline_manager_) {
    bookmarkbar_preload_pipeline_manager_.reset();
    bookmarkbar_preload_pipeline_manager_ =
        std::make_unique<BookmarkBarPreloadPipelineManager>(new_contents);
  }

  if (new_tab_page_preload_pipeline_manager_) {
    new_tab_page_preload_pipeline_manager_.reset();
    new_tab_page_preload_pipeline_manager_ =
        std::make_unique<NewTabPagePreloadPipelineManager>(new_contents);
  }

  if (glic_selection_observer_) {
    glic_selection_observer_.reset();
    glic_selection_observer_ =
        std::make_unique<glic::GlicSelectionObserver>(new_contents);
  }

  if (omnibox_autofill_bubble_controller_) {
    omnibox_autofill_bubble_controller_.reset();
    omnibox_autofill_bubble_controller_ =
        GetUserDataFactory()
            .CreateInstance<autofill::OmniboxAutofillBubbleController>(
                *tab, *tab, new_contents);
  }

  if (payments_churned_users_bubble_controller_) {
    payments_churned_users_bubble_controller_.reset();
    payments_churned_users_bubble_controller_ =
        GetUserDataFactory()
            .CreateInstance<autofill::PaymentsChurnedUsersBubbleController>(
                *tab, *tab, new_contents);
  }

  if (wallet_reminder_notice_bubble_controller_) {
    wallet_reminder_notice_bubble_controller_.reset();
    wallet_reminder_notice_bubble_controller_ =
        GetUserDataFactory()
            .CreateInstance<autofill::WalletReminderNoticeBubbleController>(
                *tab, *tab, new_contents);
  }

  if (web_payments_observer_) {
    web_payments_observer_ =
        std::make_unique<payments::WebPaymentsObserver>(new_contents);
  }

  uma_browsing_activity_tab_helper_ =
      std::make_unique<UMABrowsingActivityTabHelper>(new_contents);

  window_management_content_setting_observer_ =
      std::make_unique<web_app::WindowManagementContentSettingObserver>(
          new_contents);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_CHROMEOS)
  if (saas_usage_navigation_observer_) {
    saas_usage_navigation_observer_ =
        std::make_unique<enterprise_reporting::SaasUsageNavigationObserver>(
            new_contents);
  }
  if (hats_helper_) {
    hats_helper_ = std::make_unique<HatsHelper>(new_contents);
  }
  shared_highlighting_promo_ =
      std::make_unique<SharedHighlightingPromo>(new_contents);
#endif

#if BUILDFLAG(IS_WIN)
  font_prewarmer_tab_helper_ =
      std::make_unique<FontPrewarmerTabHelper>(new_contents);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  desktop_session_duration_observer_ =
      metrics::DesktopSessionDurationObserver::MaybeCreate(new_contents);
#endif

#if BUILDFLAG(IS_CHROMEOS)
  google_one_offer_iph_tab_helper_ =
      std::make_unique<GoogleOneOfferIphTabHelper>(new_contents);
  if (campaigns_manager_session_tab_helper_) {
    campaigns_manager_session_tab_helper_ =
        std::make_unique<CampaignsManagerSessionTabHelper>(new_contents);
  }
  cros_isolated_web_app_enabler_ =
      std::make_unique<ash::CrosIsolatedWebAppEnabler>(new_contents);
  gemini_app_tab_helper_ = GeminiAppTabHelper::MaybeCreate(new_contents);
  mahi_tab_helper_ = mahi::MahiTabHelper::MaybeCreate(new_contents);
  if (web_time_navigation_observer_) {
    web_time_navigation_observer_->OnDiscardContents(new_contents);
  }
  boot_times_recorder_tab_helper_ =
      ash::BootTimesRecorderTabHelper::MaybeCreate(new_contents);
  policy::DlpContentTabHelper::MaybeCreateForWebContents(new_contents);
#endif

#if BUILDFLAG(ENABLE_RLZ)
  chrome_rlz_tracker_web_contents_observer_ =
      ChromeRLZTrackerWebContentsObserver::MaybeCreate(new_contents);
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
  plugin_observer_.reset();
  plugin_observer_ = GetUserDataFactory().CreateInstance<PluginObserver>(
      *tab, *tab, new_contents);
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  search_ai_mode_promo_tab_helper_.reset();
  if (base::FeatureList::IsEnabled(switches::kEnableSearchAIModeSigninPromo) &&
      base::FeatureList::IsEnabled(contextual_tasks::kContextualTasks)) {
    search_ai_mode_promo_tab_helper_ =
        GetUserDataFactory()
            .CreateInstance<contextual_tasks::SearchAiModePromoTabHelper>(
                *tab, *tab, new_contents);
  }
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  captive_portal::CaptivePortalTabHelper::CreateForWebContents(
      new_contents, CaptivePortalServiceFactory::GetForProfile(profile),
      base::BindRepeating(
          &ChromeSecurityBlockingPageFactory::OpenLoginTabForWebContents,
          new_contents, false));
#endif

#if BUILDFLAG(ENABLE_COMPOSE)
  compose_client_.reset();
  if (!profile->IsOffTheRecord() &&
      autofill::ContentAutofillClient::FromWebContents(new_contents)) {
    compose_client_ = GetUserDataFactory().CreateInstance<ChromeComposeClient>(
        *tab, *tab, new_contents);
  }
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  app_tab_helper_.reset();
  app_tab_helper_ =
      std::make_unique<extensions::AppTabHelper>(*tab, new_contents);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (extensions::GetViewType(new_contents) ==
      extensions::mojom::ViewType::kInvalid) {
    extensions::SetViewType(new_contents,
                            extensions::mojom::ViewType::kTabContents);
  }
  extensions::WebNavigationTabObserver::CreateForWebContents(new_contents);
  extensions::TabHelper::CreateForWebContents(new_contents);
  navigation_extension_enabler_ =
      std::make_unique<extensions::NavigationExtensionEnabler>(new_contents);
#endif

  if (glic_marketing_page_tab_helper_) {
    glic_marketing_page_tab_helper_ =
        std::make_unique<glic::GlicMarketingPageTabHelper>(new_contents);
  }

  tab_context_decryption_token_tab_helper_ =
      TabContextDecryptionTokenTabHelper::MaybeCreate(new_contents);

  v8_compile_hints_tab_helper_ =
      v8_compile_hints::V8CompileHintsTabHelper::MaybeCreate(new_contents);

  storage_access_api_tab_helper_ = std::make_unique<StorageAccessAPITabHelper>(
      new_contents,
      StorageAccessAPIServiceFactory::GetForBrowserContext(profile));

  if (auto* service =
          RevokedPermissionsServiceFactory::GetForProfile(profile)) {
    revoked_permissions_tab_helper_ =
        std::make_unique<RevokedPermissionsTabHelper>(new_contents, service);
  }

  external_protocol_observer_ =
      std::make_unique<ExternalProtocolObserver>(new_contents);

  no_state_prefetch_tab_helper_ =
      std::make_unique<prerender::NoStatePrefetchTabHelper>(new_contents);

  navigation_predictor_preconnect_client_.reset();
  navigation_predictor_preconnect_client_ =
      std::make_unique<NavigationPredictorPreconnectClient>(new_contents);

  navigation_metrics_recorder_ =
      std::make_unique<NavigationMetricsRecorder>(new_contents);

  site_protection_metrics_observer_ =
      std::make_unique<site_protection::SiteProtectionMetricsObserver>(
          new_contents);

  permissions::PermissionRequestManager::CreateForWebContents(new_contents);

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  safe_browsing::SafeBrowsingNavigationObserver::MaybeCreateForWebContents(
      new_contents, HostContentSettingsMapFactory::GetForProfile(profile),
      safe_browsing::SafeBrowsingNavigationObserverManagerFactory::
          GetForBrowserContext(profile),
      profile->GetPrefs(), g_browser_process->safe_browsing_service(),
      enterprise_connectors::IsReferrerChainNeededForEnterprise(profile));
  safe_browsing_tab_observer_.reset();
  if (autofill::ContentAutofillClient::FromWebContents(new_contents)) {
    ChromePasswordReuseDetectionManagerClient::CreateForWebContents(
        new_contents);
    safe_browsing_tab_observer_ =
        GetUserDataFactory()
            .CreateInstance<safe_browsing::SafeBrowsingTabObserver>(
                *tab, *tab, new_contents,
                std::make_unique<
                    safe_browsing::ChromeSafeBrowsingTabObserverDelegate>());
  }
  if (tailored_security_url_observer_) {
    tailored_security_url_observer_.reset();
    tailored_security_url_observer_ =
        std::make_unique<safe_browsing::TailoredSecurityUrlObserver>(
            new_contents,
            safe_browsing::TailoredSecurityServiceFactory::GetForProfile(
                profile));
  }
  trigger_creator_.reset();
  trigger_creator_ = std::make_unique<safe_browsing::TriggerCreator>(
      *tab, profile, new_contents);
#endif

  if (about_this_site_tab_helper_) {
    about_this_site_tab_helper_.reset();
    if (auto* optimization_guide_decider =
            OptimizationGuideKeyedServiceFactory::GetForProfile(profile)) {
      about_this_site_tab_helper_ =
          GetUserDataFactory().CreateInstance<AboutThisSiteTabHelper>(
              *tab, *tab, new_contents, optimization_guide_decider);
    }
  }

  sound_content_setting_observer_.reset();
  sound_content_setting_observer_ =
      GetUserDataFactory().CreateInstance<SoundContentSettingObserver>(
          *tab, *tab, new_contents);

  task_tab_helper_.reset();
  task_tab_helper_ = GetUserDataFactory().CreateInstance<tasks::TaskTabHelper>(
      *tab, *tab, new_contents);

  if (history_embeddings_tab_helper_) {
    history_embeddings_tab_helper_ =
        std::make_unique<HistoryEmbeddingsTabHelper>(new_contents);
  }

  download_navigation_observer_ =
      std::make_unique<download::DownloadNavigationObserver>(
          new_contents, download::NavigationMonitorFactory::GetForKey(
                            profile->GetProfileKey()));

  web_contents_top_sites_observer_ =
      std::make_unique<history::WebContentsTopSitesObserver>(
          new_contents, TopSitesFactory::GetForProfile(profile).get());

  client_hints_web_contents_observer_ =
      std::make_unique<client_hints::ClientHintsWebContentsObserver>(
          new_contents);

  chained_back_navigation_tracker_.reset();
  chained_back_navigation_tracker_ =
      GetUserDataFactory().CreateInstance<ChainedBackNavigationTracker>(
          *tab, *tab, new_contents);

  FileSystemAccessPermissionRequestManager::CreateForWebContents(new_contents);

  file_system_access_tab_helper_ =
      std::make_unique<FileSystemAccessTabHelper>(new_contents);

  search_engine_tab_helper_.reset();
  search_engine_tab_helper_ =
      GetUserDataFactory().CreateInstance<SearchEngineTabHelper>(*tab, *tab,
                                                                 new_contents);

  net_error_tab_helper_.reset();
  net_error_tab_helper_ =
      GetUserDataFactory()
          .CreateInstance<chrome_browser_net::NetErrorTabHelper>(*tab, *tab,
                                                                 new_contents);

  if (supervised_user_navigation_observer_) {
    supervised_user_navigation_observer_.reset();
    supervised_user_navigation_observer_ =
        GetUserDataFactory().CreateInstance<SupervisedUserNavigationObserver>(
            *tab, *tab, new_contents);
  }

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  offline_page_tab_helper_.reset();
  offline_page_tab_helper_ =
      GetUserDataFactory().CreateInstance<offline_pages::OfflinePageTabHelper>(
          *tab, *tab, new_contents);
  recent_tab_helper_.reset();
  recent_tab_helper_ =
      GetUserDataFactory().CreateInstance<offline_pages::RecentTabHelper>(
          *tab, *tab, new_contents);
#endif

  webapps::PreRedirectionURLObserver::CreateForWebContents(new_contents);

  if (search_tab_helper_) {
    search_tab_helper_.reset();
    search_tab_helper_ = GetUserDataFactory().CreateInstance<SearchTabHelper>(
        *tab, *tab, new_contents);
  }

  auto_picture_in_picture_tab_helper_.reset();
  auto_picture_in_picture_tab_helper_ =
      GetUserDataFactory().CreateInstance<AutoPictureInPictureTabHelper>(
          *tab, *tab, new_contents);

  manage_passwords_ui_controller_.reset();
  tab_dialogs_.reset();
  tab_dialogs_ = TabDialogs::Create(*tab, new_contents);
  manage_passwords_ui_controller_ =
      GetUserDataFactory().CreateInstance<ManagePasswordsUIController>(
          *tab, *tab, new_contents);

  if (chrome_facilitated_payments_client_) {
    chrome_facilitated_payments_client_.reset();
    auto* optimization_guide_decider =
        OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
    if (autofill::ContentAutofillClient::FromWebContents(new_contents) &&
        optimization_guide_decider) {
      chrome_facilitated_payments_client_ =
          GetUserDataFactory().CreateInstance<ChromeFacilitatedPaymentsClient>(
              *tab, *tab, new_contents, optimization_guide_decider);
    }
  }

  commerce_tab_helper_.reset();
  commerce_tab_helper_ = std::make_unique<commerce::CommerceTabHelper>(
      new_contents, profile->IsOffTheRecord(),
      commerce::ShoppingServiceFactory::GetForBrowserContext(profile),
      ISOLATED_WORLD_ID_CHROME_INTERNAL);

  if (auto* metrics_services_manager =
          g_browser_process->GetMetricsServicesManager()) {
    metrics_services_web_contents_observer_ =
        std::make_unique<metrics::MetricsServicesWebContentsObserver>(
            new_contents, metrics_services_manager->GetOnDidStartLoadingCb(),
            metrics_services_manager->GetOnDidStopLoadingCb(),
            metrics_services_manager->GetOnRendererUnresponsiveCb());
  }

  popup_opener_tab_helper_.reset();
  popup_opener_tab_helper_ =
      GetUserDataFactory()
          .CreateInstance<blocked_content::PopupOpenerTabHelper>(
              *tab, *tab, new_contents, base::DefaultTickClock::GetInstance(),
              HostContentSettingsMapFactory::GetForProfile(profile));

  page_content_annotations_web_contents_observer_.reset();
  if (auto* page_content_annotations_service =
          PageContentAnnotationsServiceFactory::GetForProfile(profile)) {
    page_content_annotations_web_contents_observer_ =
        GetUserDataFactory()
            .CreateInstance<page_content_annotations::
                                PageContentAnnotationsWebContentsObserver>(
                *tab, *tab, new_contents, *page_content_annotations_service);
  }

  core_tab_helper_.reset();
  core_tab_helper_ = GetUserDataFactory().CreateInstance<CoreTabHelper>(
      *tab, *tab, new_contents);

  recently_audible_helper_.reset();
  recently_audible_helper_ =
      GetUserDataFactory().CreateInstance<RecentlyAudibleHelper>(*tab, *tab,
                                                                 new_contents);

  FindBarState::ConfigureWebContents(new_contents);

  if (MediaEngagementService::IsEnabled()) {
    MediaEngagementService::CreateWebContentsObserver(new_contents);
  }

#if BUILDFLAG(ENABLE_PRINTING)
  printing::InitializePrintingForWebContents(new_contents);
#endif

  mixed_content_settings_tab_helper_.reset();
  mixed_content_settings_tab_helper_ =
      GetUserDataFactory().CreateInstance<MixedContentSettingsTabHelper>(
          *tab, *tab, new_contents);

  permissions::PermissionRecoverySuccessRateTracker::CreateForWebContents(
      new_contents);

  javascript_dialogs::TabModalDialogManager::CreateForWebContents(
      new_contents,
      std::make_unique<JavaScriptTabModalDialogManagerDelegateDesktop>(
          new_contents));

  web_modal::WebContentsModalDialogManager::CreateForWebContents(new_contents);

  TrustedVaultEncryptionKeysTabHelper::CreateForWebContents(new_contents);

  one_time_permissions_tracker_helper_.reset();
  one_time_permissions_tracker_helper_ =
      std::make_unique<OneTimePermissionsTrackerHelper>(new_contents);

  if (loading_predictor_tab_helper_) {
    loading_predictor_tab_helper_.reset();
    loading_predictor_tab_helper_ =
        GetUserDataFactory()
            .CreateInstance<predictors::LoadingPredictorTabHelper>(
                *tab, *tab, new_contents);
  }

  if (breadcrumbs::IsEnabled(g_browser_process->local_state())) {
    BreadcrumbManagerTabHelper::CreateForWebContents(new_contents);
  }

  PrefsTabHelper::CreateForWebContents(new_contents);

  if (site_engagement::SiteEngagementService::IsEnabled()) {
    site_engagement::SiteEngagementService::Helper::CreateForWebContents(
        new_contents,
        prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(
            profile));
  }

  SafetyTipWebContentsObserver::CreateForWebContents(new_contents);

  HttpsOnlyModeTabHelper::CreateForWebContents(new_contents);

  login_detection::LoginDetectionTabHelper::MaybeCreateForWebContents(
      new_contents);

  if (!profile->IsOffTheRecord()) {
    HistoryClustersTabHelper::CreateForWebContents(
        new_contents, HistoryTabHelper::FromWebContents(new_contents));
  }

  blocked_content::PopupBlockerTabHelper::CreateForWebContents(new_contents);

  resource_coordinator::ResourceCoordinatorTabHelper::CreateForWebContents(
      new_contents);

  ukm::InitializeSourceUrlRecorderForWebContents(new_contents);
}

customize_chrome::SidePanelController*
TabFeatures::SetCustomizeChromeSidePanelControllerForTesting(
    std::unique_ptr<customize_chrome::SidePanelController>
        customize_chrome_side_panel_controller) {
  customize_chrome_side_panel_controller_ =
      std::move(customize_chrome_side_panel_controller);
  return customize_chrome_side_panel_controller_.get();
}

TabAlertController* TabFeatures::SetTabAlertControllerForTesting(
    std::unique_ptr<TabAlertController> tab_alert_controller) {
  tab_alert_controller_ = std::move(tab_alert_controller);
  return tab_alert_controller_.get();
}

// static
ui::UserDataFactoryWithOwner<TabInterface>& TabFeatures::GetUserDataFactory() {
  static base::NoDestructor<ui::UserDataFactoryWithOwner<TabInterface>> factory;
  return *factory;
}

// static
ui::UserDataFactoryWithOwner<TabInterface>&
TabFeatures::GetUserDataFactoryForTesting() {
  return GetUserDataFactory();
}

}  // namespace tabs
