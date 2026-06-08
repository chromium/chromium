// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/browser_window_features.h"

#include <memory>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chrome/browser/actor/ui/actor_border_view_controller.h"
#include "chrome/browser/actor/ui/actor_ui_window_controller.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/collaboration/collaboration_service_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_controller.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider_impl.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"
#include "chrome/browser/devtools/devtools_ui_controller.h"
#include "chrome/browser/enterprise/data_protection/data_protection_ui_controller.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/glic/browser_ui/glic_actor_nudge_controller.h"
#include "chrome/browser/glic/browser_ui/glic_button_controller.h"
#include "chrome/browser/glic/browser_ui/glic_iph_controller.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_controller.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_controller_desktop.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/lens/region_search/lens_region_search_controller.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/skills/skills_ui_window_controller.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/ai_overlay_dialog/ai_overlay_dialog_controller.h"
#include "chrome/browser/ui/animation/browser_animation_controller.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar_controller.h"
#include "chrome/browser/ui/breadcrumb_manager_browser_agent.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "chrome/browser/ui/browser_location_bar_model_delegate.h"
#include "chrome/browser/ui/browser_select_file_dialog_controller.h"
#include "chrome/browser/ui/browser_tab_menu_model_delegate.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"
#include "chrome/browser/ui/browser_window/public/embedder_browser_window_features.h"
#include "chrome/browser/ui/browser_window_theme_observer.h"
#include "chrome/browser/ui/call_to_action/call_to_action_lock.h"
#include "chrome/browser/ui/context_highlight/context_highlight_window_feature.h"
#include "chrome/browser/ui/contextual_search/searchbox_context_data.h"
#include "chrome/browser/ui/desktop_to_mobile_promos/ios_promo_controller.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/extensions/extension_installed_watcher.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/focus/browser_focus_controller.h"
#include "chrome/browser/ui/focus/browser_focus_controller_views.h"
#include "chrome/browser/ui/focus/browser_focus_controller_webui.h"
#include "chrome/browser/ui/fullscreen/browser_window_fullscreen_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_entry_point_controller.h"
#include "chrome/browser/ui/omnibox/ai_mode_page_action_controller.h"
#include "chrome/browser/ui/performance_controls/memory_saver_bubble_controller.h"
#include "chrome/browser/ui/performance_controls/memory_saver_opt_in_iph_controller.h"
#include "chrome/browser/ui/sessions/session_service_browser_helper.h"
#include "chrome/browser/ui/sharing_hub/sharing_hub_window_controller.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/sync/browser_synced_window_delegate.h"
#include "chrome/browser/ui/tab_search_feature.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/projects/projects_panel_state_controller.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/most_recent_shared_tab_update_store.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/session_service_tab_group_sync_observer.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/shared_tab_group_feedback_controller.h"
#include "chrome/browser/ui/tabs/split_tab_highlight_controller.h"
#include "chrome/browser/ui/tabs/split_view_iph_controller.h"
#include "chrome/browser/ui/tabs/tab_drag_api/desktop_tab_drag_impl/tab_drag_window_adapter_impl.h"
#include "chrome/browser/ui/tabs/tab_drag_api/tab_drag_service_feature.h"
#include "chrome/browser/ui/tabs/tab_group_deletion_dialog_controller.h"
#include "chrome/browser/ui/tabs/tab_list_bridge.h"
#include "chrome/browser/ui/tabs/tab_strip_api/controllers/tab_strip_ui_controller_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_api/controllers/tab_strip_ui_controller_injector_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_model_impl/tab_strip_model_injector.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_feature.h"
#include "chrome/browser/ui/tabs/tab_strip_prefs.h"
#include "chrome/browser/ui/tabs/vertical_tab_iph_controller.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/browser/ui/toasts/toast_service.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_utils.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/tab_search_toolbar_button_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/unload_controller.h"
#include "chrome/browser/ui/views/animations/side_panel_animations.h"
#include "chrome/browser/ui/views/animations/tab_strip_animations.h"
#include "chrome/browser/ui/views/color_provider_browser_helper.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_close_button_controller.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_ephemeral_button_controller.h"
#include "chrome/browser/ui/views/data_sharing/data_sharing_bubble_controller.h"
#include "chrome/browser/ui/views/extensions/extension_keybinding_registry_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_border_controller.h"
#include "chrome/browser/ui/views/frame/find_bar_owner_views.h"
#include "chrome/browser/ui/views/frame/horizontal_tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/browser/ui/views/frame/scrim_view_controller.h"
#include "chrome/browser/ui/views/fullscreen_control/fullscreen_control_host.h"
#include "chrome/browser/ui/views/incognito_clear_browsing_data_dialog_coordinator.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views_impl.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_coordinator.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_coordinator.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_manager_views.h"
#include "chrome/browser/ui/views/media_router/cast_browser_controller.h"
#include "chrome/browser/ui/views/new_tab_footer/footer_controller.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_closer.h"
#include "chrome/browser/ui/views/profiles/profile_customization_bubble_sync_controller.h"
#include "chrome/browser/ui/views/profiles/profile_menu_coordinator.h"
#include "chrome/browser/ui/views/qrcode_generator/qrcode_window_controller.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_bubble_controller.h"
#include "chrome/browser/ui/views/sharing/sharing_window_controller.h"
#include "chrome/browser/ui/views/side_panel/bookmarks/bookmarks_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/comments/comments_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_manager.h"
#include "chrome/browser/ui/views/side_panel/history/history_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/history_clusters/history_clusters_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/reading_list/reading_list_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/tabs_from_other_devices/tabs_from_other_devices_side_panel_coordinator.h"
#include "chrome/browser/ui/views/tabs/groups/recent_activity_bubble_dialog_view.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_utils.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_coordinator.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/translate/translate_bubble_controller.h"
#include "chrome/browser/ui/views/upgrade_notification_controller.h"
#include "chrome/browser/ui/views/user_education/impl/browser_user_education_interface_impl.h"
#include "chrome/browser/ui/waap/initial_web_ui_manager.h"
#include "chrome/browser/ui/waap/initial_webui_window_metrics_manager.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/ui/web_modal/browser_window_modal_dialog_delegate.h"
#include "chrome/browser/ui/webui_browser/browser_elements_webui_browser.h"
#include "chrome/browser/ui/webui_browser/find_bar_owner_webui_browser.h"
#include "chrome/browser/ui/webui_browser/webui_browser.h"
#include "chrome/browser/ui/webui_browser/webui_browser_exclusive_access_context.h"
#include "chrome/browser/ui/webui_browser/webui_browser_side_panel_ui.h"
#include "chrome/browser/ui/webui_browser/webui_browser_window.h"
#include "chrome/browser/ui/webui_browser/zoom_bubble_manager_webui_browser.h"
#include "chrome/browser/ui/window_feature_controller/window_feature_controller.h"
#include "chrome/browser/ui/window_metadata/window_metadata_controller.h"
#include "chrome/browser/ui/zoom/browser_window_zoom_observer.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/breadcrumbs/core/breadcrumbs_status.h"
#include "components/collaboration/public/collaboration_service.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/feature_utils.h"
#include "components/commerce/core/shopping_service.h"
#include "components/content_settings/browser/ui/cookie_controls_controller.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/contextual_tasks/public/features.h"
#include "components/desktop_to_mobile_promos/features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/lens/lens_features.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/omnibox/browser/location_bar_model_impl.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/search/ntp_features.h"
#include "components/search/search.h"
#include "content/public/common/content_constants.h"
#include "extensions/browser/manifest_v2_experiment_manager.h"
#include "extensions/common/extension_features.h"
#include "ui/views/interaction/element_highlighter_views.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_browser_window_helper.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/search_engines/default_search_extension_controlled_controller.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/pdf/infobar/pdf_infobar_controller.h"
#include "chrome/browser/ui/startup/default_browser_prompt/pin_infobar/pin_infobar_controller.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_controller.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/views/frame/windows_taskbar_icon_updater.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/boca/on_task/on_task_locked_controller.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/bubble/download_display_controller.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_ui_controller.h"
#endif

#if defined(USE_AURA)
#include "chrome/browser/ui/overscroll_pref_manager.h"
#endif  // defined(USE_AURA)

BrowserWindowFeatures::BrowserWindowFeatures() = default;
BrowserWindowFeatures::~BrowserWindowFeatures() = default;

void BrowserWindowFeatures::Init(BrowserWindowInterface* browser) {
  // This is used only for the controllers which will be created on demand
  // later.
  browser_ = browser;

  // `InitialWebUIWindowMetricsManager` depends on Browser (for Profile) and
  // must be initialized before BrowserView creation because it is used by
  // various views which are created during BrowserView::Init.
  if (waap::IsInitialWebUIMetricsLoggingEnabled()) {
    initial_webui_window_metrics_manager_ =
        std::make_unique<InitialWebUIWindowMetricsManager>(browser);
  }

  searchbox_context_data_ = std::make_unique<SearchboxContextData>();

  app_browser_controller_ =
      GetUserDataFactory().CreateInstanceWithFactoryMethod(
          *browser, &web_app::MaybeCreateAppBrowserController, browser);

  fullscreen_controller_ =
      std::make_unique<BrowserWindowFullscreenController>(*browser);

  browser_actions_ = std::make_unique<BrowserActions>(browser);

  window_feature_controller_ =
      GetUserDataFactory().CreateInstance<WindowFeatureController>(
          *browser, fullscreen_controller_.get(), app_browser_controller_.get(),
          browser->GetType(),
          browser->GetBrowserForMigrationOnly()->is_trusted_source(),
          browser->GetUnownedUserDataHost());

  immersive_mode_controller_ =
      GetUserDataFactory()
          .CreateInstanceWithFactoryMethod<ImmersiveModeController,
                                           WindowFeatureController*,
                                           ui::UnownedUserDataHost&>(
              *browser_, &chrome::CreateImmersiveModeController,
              window_feature_controller_.get(),
              browser->GetUnownedUserDataHost());

  browser_command_controller_ =
      std::make_unique<chrome::BrowserCommandController>(browser);

  browser_actions_->InitializeBrowserActions();

  if (webui_browser::IsWebUIBrowserEnabled() &&
      browser->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL) {
    browser_elements_ =
        GetUserDataFactory().CreateInstance<BrowserElementsWebUiBrowser>(
            *browser, *browser);
  } else {
    browser_elements_ =
        GetUserDataFactory().CreateInstance<BrowserElementsViewsImpl>(*browser,
                                                                      *browser);
  }

  ui::ElementHighlighter::GetElementHighlighter()
      ->MaybeRegisterBackend<views::ElementHighlighterViews>();

  // Initialize bookmark bar controller for all browser types.
  bookmark_bar_controller_ =
      GetUserDataFactory().CreateInstance<BookmarkBarController>(
          *browser, *browser, *browser->GetTabStripModel());

  session_service_browser_helper_ =
      std::make_unique<SessionServiceBrowserHelper>(
          browser->GetTabStripModel(), browser->GetSessionID(),
          browser->GetType(), browser->GetProfile());

  tab_strip_model_ = browser->GetTabStripModel();
  tab_list_bridge_ = std::make_unique<TabListBridge>(
      *tab_strip_model_, browser->GetUnownedUserDataHost());

  // Avoid passing `browser` directly to features. Instead, pass the minimum
  // necessary state or controllers necessary.
  // Ping erikchen for assistance. This comment will be deleted after there are
  // 10+ features.
  //
  // Features that are only enabled for normal browser windows (e.g. a window
  // with an omnibox and a tab strip). By default most features should be
  // instantiated in this block.
  Profile* const profile = browser->GetProfile();
  if (browser->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL) {
    initial_web_ui_manager_ = std::make_unique<InitialWebUIManager>(browser);

    if (search::IsInstantExtendedAPIEnabled()) {
      instant_controller_ = std::make_unique<BrowserInstantController>(
          profile, browser->GetTabStripModel());
    }

    if (profile->IsRegularProfile() &&
        browser->GetTabStripModel()->SupportsTabGroups() &&
        tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile)) {
      session_service_tab_group_sync_observer_ =
          std::make_unique<tab_groups::SessionServiceTabGroupSyncObserver>(
              profile, browser->GetTabStripModel(), browser->GetSessionID());

      most_recent_shared_tab_update_store_ =
          std::make_unique<tab_groups::MostRecentSharedTabUpdateStore>(browser);
    }

    if (base::FeatureList::IsEnabled(contextual_cueing::kContextualCueingV2)) {
      contextual_cueing_controller_ =
          std::make_unique<contextual_cueing::ContextualCueingController>(
              browser, tab_list_bridge_.get());
    }

    if (glic::GlicEnabling::IsProfileEligible(profile)) {
      glic_iph_controller_ = std::make_unique<glic::GlicIphController>(
          browser, *glic::GlicKeyedService::Get(profile));
      glic_nudge_controller_ =
          std::make_unique<glic::GlicNudgeControllerDesktop>(
              browser, tab_list_bridge_.get());
    }

    if (tabs::IsVerticalTabsFeatureEnabled()) {
      Browser* raw_browser = browser->GetBrowserForMigrationOnly();

      std::optional<bool> restored_state_collapsed =
          raw_browser->is_vertical_tabs_initially_collapsed();
      std::optional<int> restored_state_uncollapsed_width =
          raw_browser->get_vertical_tabs_initial_uncollapsed_width();

      if (!restored_state_collapsed.has_value() &&
          !restored_state_uncollapsed_width.has_value() &&
          !browser->CreatedBySessionRestore()) {
        restored_state_collapsed =
            profile->GetPrefs()->GetBoolean(prefs::kVerticalTabsCollapsedState);
        restored_state_uncollapsed_width = profile->GetPrefs()->GetInteger(
            prefs::kVerticalTabsUncollapsedWidth);
      }

      vertical_tab_strip_state_controller_ =
          GetUserDataFactory()
              .CreateInstance<tabs::VerticalTabStripStateController>(
                  *browser, browser, profile->GetPrefs(),
                  browser_actions_->root_action_item(),
                  SessionServiceFactory::GetForProfile(browser_->GetProfile()),
                  browser_->GetSessionID(), restored_state_collapsed,
                  restored_state_uncollapsed_width);
    }

    if (projects_panel::IsProjectsPanelVisibleForProfile(profile)) {
      glic::GlicKeyedService* glic_service =
          glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile);
      projects_panel_state_controller_ =
          GetUserDataFactory().CreateInstance<ProjectsPanelStateController>(
              *browser, browser, browser_actions_->root_action_item(),
              AimEligibilityServiceFactory::GetForProfile(profile),
              glic_service ? &glic_service->enabling() : nullptr);
    }

    if (base::FeatureList::IsEnabled(features::kAiOverlayDialog)) {
      ai_overlay_dialog_controller_ =
          GetUserDataFactory().CreateInstance<ttc::AiOverlayDialogController>(
              *browser, browser);
    }
  }

  // The LensOverlayEntryPointController is constructed for all browser types
  // but is only initialized for normal browser windows. This simplifies the
  // logic for code shared by both normal and non-normal windows.
  lens_overlay_entry_point_controller_ =
      GetUserDataFactory()
          .CreateInstance<lens::LensOverlayEntryPointController>(*browser,
                                                                 browser);
  lens_region_search_controller_ =
      std::make_unique<lens::LensRegionSearchController>();

  tab_strip_service_feature_ = std::make_unique<TabStripServiceFeature>(
      std::make_unique<tabs_api::tab_strip_model::TabStripModelInjector>(
          browser, tab_strip_model_));

  auto adapter = std::make_unique<TabDragWindowAdapterImpl>(browser);
  tab_drag_service_feature_ =
      std::make_unique<TabDragServiceFeature>(std::move(adapter));

  tab_strip_ui_controller_ =
      std::make_unique<tabs_api::TabStripUIControllerImpl>(
          std::make_unique<tabs_api::TabStripUIControllerInjectorImpl>(
              browser, tab_strip_model_));

  memory_saver_bubble_controller_ =
      std::make_unique<memory_saver::MemorySaverBubbleController>(browser);

  translate_bubble_controller_ =
      GetUserDataFactory().CreateInstance<TranslateBubbleController>(
          *browser, browser, browser_actions_->root_action_item());

  cookie_controls_controller_ =
      std::make_unique<content_settings::CookieControlsController>(
          CookieSettingsFactory::GetForProfile(profile),
          profile->IsOffTheRecord() ? CookieSettingsFactory::GetForProfile(
                                          profile->GetOriginalProfile())
                                    : nullptr,
          HostContentSettingsMapFactory::GetForProfile(profile),
          profile->IsIncognitoProfile());

  cookie_controls_bubble_coordinator_ =
      GetUserDataFactory().CreateInstance<CookieControlsBubbleCoordinator>(
          *browser, browser, browser_actions_->root_action_item());

  tab_menu_model_delegate_ =
      std::make_unique<chrome::BrowserTabMenuModelDelegate>(
          browser->GetSessionID(), profile, app_browser_controller_.get(),
          tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile));

  tab_group_deletion_dialog_controller_ =
      std::make_unique<tab_groups::DeletionDialogController>(browser, profile,
                                                             tab_strip_model_);

  user_education_ =
      GetUserDataFactory().CreateInstance<BrowserUserEducationInterfaceImpl>(
          *browser, browser);

  location_bar_model_delegate_ =
      std::make_unique<BrowserLocationBarModelDelegate>(tab_strip_model_);
  location_bar_model_ = std::make_unique<LocationBarModelImpl>(
      location_bar_model_delegate_.get(), content::kMaxURLDisplayChars);

  side_panel_registry_ =
      GetUserDataFactory().CreateInstance<SidePanelRegistry>(*browser, browser);

  reading_list_side_panel_coordinator_ =
      GetUserDataFactory().CreateInstance<ReadingListSidePanelCoordinator>(
          *browser, browser, profile, browser->GetTabStripModel());

  if (TabsFromOtherDevicesSidePanelCoordinator::IsSupported(profile)) {
    tabs_from_other_devices_side_panel_coordinator_ =
        std::make_unique<TabsFromOtherDevicesSidePanelCoordinator>(browser,
                                                                   profile);
  }

  bookmarks_side_panel_coordinator_ =
      GetUserDataFactory().CreateInstance<BookmarksSidePanelCoordinator>(
          *browser, *browser);

  if (base::FeatureList::IsEnabled(contextual_tasks::kContextualTasks)) {
    contextual_tasks_active_task_context_provider_ =
        std::make_unique<contextual_tasks::ActiveTaskContextProviderImpl>(
            browser_,
            contextual_tasks::ContextualTasksServiceFactory::GetForProfile(
                browser_->GetProfile()));
    contextual_tasks_entry_point_eligibility_manager_ =
        GetUserDataFactory()
            .CreateInstance<contextual_tasks::EntryPointEligibilityManager>(
                *browser_, browser_);
    contextual_tasks_side_panel_coordinator_ =
        GetUserDataFactory()
            .CreateInstance<
                contextual_tasks::ContextualTasksSidePanelCoordinator>(
                *browser_, browser_,
                contextual_tasks_active_task_context_provider_.get(),
                contextual_tasks_entry_point_eligibility_manager_.get());

    if (contextual_tasks::kShowEntryPoint.Get() ==
        contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) {
      contextual_tasks_ephemeral_button_controller_ =
          GetUserDataFactory()
              .CreateInstance<ContextualTasksEphemeralButtonController>(
                  *browser_, browser_);
    }

    contextual_tasks_close_button_controller_ =
        GetUserDataFactory()
            .CreateInstance<ContextualTasksCloseButtonController>(
                *browser_, browser_,
                contextual_tasks_entry_point_eligibility_manager_.get(),
                contextual_tasks_side_panel_coordinator_.get());
  }

  signin_view_controller_ = std::make_unique<SigninViewController>(
      browser, profile, tab_strip_model_);

  extension_installed_watcher_ =
      std::make_unique<ExtensionInstalledWatcher>(profile);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  if (base::FeatureList::IsEnabled(features::kPdfInfoBar)) {
    pdf_infobar_controller_ =
        GetUserDataFactory().CreateInstance<pdf::infobar::PdfInfoBarController>(
            *browser, browser);
  }
  pin_infobar_controller_ =
      GetUserDataFactory()
          .CreateInstance<default_browser::PinInfoBarController>(*browser,
                                                                 browser);
#endif

  data_sharing_bubble_controller_ =
      std::make_unique<DataSharingBubbleController>(browser, profile,
                                                    tab_strip_model_);

  content_setting_bubble_model_delegate_ =
      std::make_unique<BrowserContentSettingBubbleModelDelegate>(browser);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extension_browser_window_helper_ =
      std::make_unique<extensions::ExtensionBrowserWindowHelper>(browser,
                                                                 profile);
#endif

  if (breadcrumbs::IsEnabled(g_browser_process->local_state())) {
    breadcrumb_manager_browser_agent_ =
        std::make_unique<BreadcrumbManagerBrowserAgent>(
            browser->GetTabStripModel(), profile);
  }

#if defined(USE_AURA)
  overscroll_pref_manager_ = std::make_unique<OverscrollPrefManager>(
      tab_strip_model_,
      browser->GetType() == BrowserWindowInterface::Type::TYPE_DEVTOOLS);
#endif  // defined(USE_AURA)

  if (base::FeatureList::IsEnabled(features::kGlicActorUi) &&
      features::kGlicActorUiBorderGlow.Get()) {
    actor_border_view_controller_ =
        std::make_unique<ActorBorderViewController>(browser);
  }

  browser_animation_controller_ =
      GetUserDataFactory().CreateInstance<BrowserAnimationController>(*browser,
                                                                      *browser);

  context_highlight_window_feature_ =
      std::make_unique<ContextHighlightWindowFeature>(*browser);

  browser_window_theme_observer_ =
      GetUserDataFactory().CreateInstance<BrowserWindowThemeObserver>(*browser,
                                                                      browser);

  browser_window_zoom_observer_ =
      GetUserDataFactory().CreateInstance<BrowserWindowZoomObserver>(*browser,
                                                                     browser);

  browser_window_modal_dialog_delegate_ =
      GetUserDataFactory().CreateInstance<BrowserWindowModalDialogDelegate>(
          *browser, browser);

  call_to_action_lock_ =
      GetUserDataFactory().CreateInstance<CallToActionLock>(*browser, browser);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  profile_customization_bubble_sync_controller_ =
      std::make_unique<ProfileCustomizationBubbleSyncController>(browser,
                                                                 profile);
  session_restore_infobar_controller_ =
      GetUserDataFactory()
          .CreateInstance<
              session_restore_infobar::SessionRestoreInfobarController>(
              *browser, browser);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS)
  on_task_locked_controller_ =
      GetUserDataFactory().CreateInstance<ash::boca::OnTaskLockedController>(
          *browser, browser);
#endif  // BUILDFLAG(IS_CHROMEOS)

  unload_controller_ = std::make_unique<UnloadController>(browser);

  window_metadata_controller_ = std::make_unique<WindowMetadataController>(
      *browser,
      browser->GetBrowserForMigrationOnly()->create_params().user_title);

  // Initialize embedder features last.
  embedder_browser_window_features_ =
      GetUserDataFactory().CreateInstance<EmbedderBrowserWindowFeatures>(
          *browser, browser);
  embedder_browser_window_features_->Init(browser);
}

void BrowserWindowFeatures::InitPostWindowConstruction(Browser* browser) {
  Profile* const profile = browser_->GetProfile();
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  WebUIBrowserWindow* const webui_browser_window =
      WebUIBrowserWindow::FromBrowser(browser);

  desktop_browser_window_capabilities_ =
      GetUserDataFactory().CreateInstance<DesktopBrowserWindowCapabilities>(
          *browser, browser_window_modal_dialog_delegate_.get(),
          unload_controller_.get(), browser->window(),
          browser->GetUnownedUserDataHost());

  // TODO(crbug.com/346148093): Move SidePanelCoordinator construction to
  // Init.
  // TODO(crbug.com/346148554): Do not create a SidePanelCoordinator for most
  // browser.h types
  // Conceptually, SidePanelCoordinator handles the "model" whereas
  // BrowserView::side_panel_ handles the "ui". When we stop
  // making this for most browser.h types, we should also stop making the
  // side_panel_.
  if (browser_view) {
    side_panel_coordinator_ =
        GetUserDataFactory().CreateInstance<SidePanelCoordinator>(*browser_,
                                                                  browser);
  } else if (webui_browser_window) {
    webui_browser_side_panel_ui_ =
        std::make_unique<WebUIBrowserSidePanelUI>(browser);
  }

  if (browser_view) {
    browser_focus_controller_ =
        GetUserDataFactory().CreateInstance<BrowserFocusControllerViews>(
            *browser, browser->GetWindow(), browser->GetUnownedUserDataHost(),
            profile, browser_elements_.get(),
            ToolbarButtonProvider::From(browser));
  } else if (WebUIBrowserWindow::FromBrowser(browser)) {
    browser_focus_controller_ =
        GetUserDataFactory().CreateInstance<BrowserFocusControllerWebUI>(
            *browser, browser->GetWindow(), browser->GetUnownedUserDataHost());
  } else {
    browser_focus_controller_ =
        GetUserDataFactory().CreateInstance<StubBrowserFocusController>(
            *browser, browser->GetWindow(), browser->GetUnownedUserDataHost());
  }

  if (webui_browser_window) {
    webui_browser_exclusive_access_context_ =
        std::make_unique<WebUIBrowserExclusiveAccessContext>(
            browser->profile(), browser_, browser->GetTabStripModel(),
            webui_browser_window->widget(), webui_browser_window);
  }

  exclusive_access_manager_ = std::make_unique<ExclusiveAccessManager>(
      browser, browser->window()->GetExclusiveAccessContext());

  // This code needs exclusive access manager to be initialized.
#if !BUILDFLAG(IS_CHROMEOS)
  if (download_toolbar_ui_controller_) {
    download_toolbar_ui_controller_->display_controller()
        ->ListenToFullScreenChanges();
  }
#endif

  if (browser_view) {
    // Initialize fullscreen control host after exclusive access manager is
    // ready.
    fullscreen_control_host_ = std::make_unique<FullscreenControlHost>(
        browser_view, exclusive_access_manager_.get());
  }

  // Features that are only enabled for normal browser windows (e.g. a window
  // with an omnibox and a tab strip). By default most features should be
  // instantiated in this block.
  if (browser->is_type_normal()) {
    if (IsChromeLabsEnabled()) {
      chrome_labs_coordinator_ =
          GetUserDataFactory().CreateInstance<ChromeLabsCoordinator>(*browser,
                                                                     browser);
    }

    if (MobilePromoOnDesktopEnabled()) {
      ios_promo_controller_ =
          GetUserDataFactory().CreateInstance<IOSPromoController>(*browser,
                                                                  browser);
    }

    send_tab_to_self_toolbar_bubble_controller_ =
        GetUserDataFactory()
            .CreateInstance<
                send_tab_to_self::SendTabToSelfToolbarBubbleController>(
                *browser, browser);

    sharing_window_controller_ =
        GetUserDataFactory().CreateInstance<SharingWindowController>(*browser,
                                                                     browser);

    sharing_hub_window_controller_ =
        GetUserDataFactory()
            .CreateInstance<sharing_hub::SharingHubWindowController>(*browser,
                                                                     browser);
    qrcode_window_controller_ =
        GetUserDataFactory()
            .CreateInstance<qrcode_generator::QRCodeWindowController>(*browser,
                                                                      browser);

    if (browser_view) {
      // Get the PinnedToolbarActions for the browser; it might not exist for
      // browsers with a custom tab toolbar.
      pinned_toolbar_actions_ =
          browser_view->toolbar_button_provider()->GetPinnedToolbarActions();
    }

    // TODO(crbug.com/350508658): Ideally, we don't pass in a reference to
    // browser as per the guidance in the comment above. However, currently,
    // we need browser to properly determine if the lens overlay is enabled.
    // Cannot be in Init since needs to listen to the fullscreen controller
    // and location bar view which are initialized after Init.
    if (lens::features::IsLensOverlayEnabled()) {
      views::View* location_bar = nullptr;
      // TODO(crbug.com/360163254): We should really be using
      // Browser::GetBrowserView, which always returns a non-null BrowserView
      // in production, but this crashes during unittests using
      // BrowserWithTestWindowTest; these should eventually be refactored.
      if (browser_view) {
        location_bar = browser_view->GetLocationBarView();
      }
      lens_overlay_entry_point_controller_->Initialize(
          browser, browser_command_controller_.get(), location_bar);
    }

    if (browser_view && IsPageActionMigrated(PageActionIconType::kAiMode)) {
      LocationBarView* location_bar_view = browser_view->GetLocationBarView();
      // TODO(crbug.com/491707187): Make it work with any LocationBar
      if (location_bar_view) {
        ai_mode_page_action_controller_ =
            GetUserDataFactory()
                .CreateInstance<omnibox::AiModePageActionController>(
                    *browser, *browser, *profile, *location_bar_view);
      }
    }

    if (browser->GetTabStripModel()->SupportsTabGroups() &&
        tab_groups::SavedTabGroupUtils::SupportsSharedTabGroups() &&
        tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile)) {
      if (browser_view) {
        shared_tab_group_feedback_controller_ =
            std::make_unique<tab_groups::SharedTabGroupFeedbackController>(
                browser_view->browser());
        shared_tab_group_feedback_controller_->Init();
      }
      recent_activity_bubble_coordinator_ =
          GetUserDataFactory().CreateInstance<RecentActivityBubbleCoordinator>(
              *browser, browser);
    }

    if (browser_view) {
      split_tab_highlight_controller_ =
          std::make_unique<split_tabs::SplitTabHighlightController>(
              browser_view->browser(), browser_view->multi_contents_view());
    }

    if (base::FeatureList::IsEnabled(
            feature_engagement::kIPHSideBySidePinnableFeature) ||
        base::FeatureList::IsEnabled(
            feature_engagement::kIPHSideBySideTabSwitchFeature)) {
      split_view_iph_controller_ =
          GetUserDataFactory().CreateInstance<SplitViewIphController>(*browser,
                                                                      browser);
    }

    if (base::FeatureList::IsEnabled(
            feature_engagement::kIPHVerticalTabstripTutorialFeature)) {
      vertical_tab_iph_controller_ =
          GetUserDataFactory().CreateInstance<VerticalTabIphController>(
              *browser, browser);
    }
  }

  synced_window_delegate_ = std::make_unique<BrowserSyncedWindowDelegate>(
      browser, browser->GetTabStripModel(), browser->GetSessionID(),
      browser->GetType());

  extension_window_controller_ =
      std::make_unique<extensions::BrowserExtensionWindowController>(browser);

  profile_menu_coordinator_ =
      std::make_unique<ProfileMenuCoordinator>(browser, profile);

  upgrade_notification_controller_ =
      std::make_unique<UpgradeNotificationController>(browser);

  incognito_clear_browsing_data_dialog_coordinator_ =
      std::make_unique<IncognitoClearBrowsingDataDialogCoordinator>(profile);

  if (browser_view) {
    color_provider_browser_helper_ =
        std::make_unique<ColorProviderBrowserHelper>(
            browser->GetTabStripModel(), browser_view->GetWidget(), browser);
  }

  live_tab_context_ = std::make_unique<BrowserLiveTabContext>(
      browser, browser->GetTabStripModel(), profile, browser->GetWindow(),
      browser->GetType(), browser->app_name(), browser->GetSessionID());

  if (browser->is_type_normal() || browser->is_type_app()) {
    toast_service_ = std::make_unique<ToastService>(browser);
  }

  views::FocusManager* focus_manager = nullptr;
  if (browser_view) {
    focus_manager = browser_view->GetFocusManager();
    contents_border_controller_ =
        std::make_unique<ContentsBorderController>(browser_view);

    // BrowserView is an AcceleratorProvider.
    accelerator_provider_ = browser_view;

    bookmark_bar_controller_->SetDelegate(browser_view);
  }

  if (webui_browser_window) {
    focus_manager = webui_browser_window->widget()->GetFocusManager();

    // WebUIBrowserWindow is an AcceleratorProvider.
    accelerator_provider_ = webui_browser_window;

    bookmark_bar_controller_->SetDelegate(webui_browser_window);

    find_bar_owner_ =
        std::make_unique<FindBarOwnerWebUIBrowser>(webui_browser_window);

    zoom_bubble_manager_ =
        std::make_unique<ZoomBubbleManagerWebUIBrowser>(webui_browser_window);
    zoom_bubble_coordinator_ =
        GetUserDataFactory().CreateInstance<ZoomBubbleCoordinator>(
            *browser_, *browser_, zoom_bubble_manager_.get());
  }

  // Focus manager can be null in tests.
  if (focus_manager) {
    extension_keybinding_registry_ =
        std::make_unique<ExtensionKeybindingRegistryViews>(
            profile, TabListInterface::From(browser),
            extensions::ExtensionKeybindingRegistry::ALL_EXTENSIONS,
            focus_manager);
  }

  browser_select_file_dialog_controller_ =
      std::make_unique<BrowserSelectFileDialogController>(
          browser->profile(), browser->tab_strip_model(), browser->window(),
          browser);

  // Initialize post-window dependent embedder features last.
  embedder_browser_window_features_->InitPostWindowConstruction(browser);
}

void BrowserWindowFeatures::InitPostBrowserViewConstruction(
    BrowserView* browser_view) {
  scrim_view_controller_ = std::make_unique<ScrimViewController>(browser_view);

  // Set the window for the animation controller. Add animation providers here
  // as well.
  browser_animation_controller_->set_browser_view(browser_view);
  browser_animation_controller_->AddAnimationProvider(
      std::make_unique<SidePanelAnimations>());
  browser_animation_controller_->AddAnimationProvider(
      std::make_unique<TabStripAnimations>());

  if (HistorySidePanelCoordinator::IsSupported()) {
    GetUserDataFactory().CreateInstance<HistorySidePanelCoordinator>(
        *browser_view->browser(), browser_view->browser());
  }

  history_clusters_side_panel_coordinator_ =
      std::make_unique<HistoryClustersSidePanelCoordinator>(
          browser_, browser_->GetProfile());

  if (CommentsSidePanelCoordinator::IsSupported()) {
    comments_side_panel_coordinator_ =
        GetUserDataFactory().CreateInstance<CommentsSidePanelCoordinator>(
            *browser_view->browser(), browser_view->browser());
  }

  extension_side_panel_manager_ =
      std::make_unique<extensions::ExtensionSidePanelManager>(
          browser_view->browser(), side_panel_registry_.get());

  if (browser_view->GetIsNormalType()) {
    glic::GlicKeyedService* glic_service =
        glic::GlicKeyedService::Get(browser_view->GetProfile());
    if (glic_service) {
      auto* tab_strip_container =
          BrowserElementsViews::From(browser_view->browser())
              ->GetViewAs<TabStripActionContainer>(
                  kTabStripActionContainerElementId);
      auto* toolbar_view =
          BrowserElementsViews::From(browser_view->browser())
              ->GetViewAs<ToolbarView>(ToolbarView::kToolbarElementId);

      if (tab_strip_container && toolbar_view) {
        glic_button_controller_ = std::make_unique<glic::GlicButtonController>(
            browser_view->GetProfile(), *browser_, tab_strip_container,
            toolbar_view, glic_service);
      }

      if (base::FeatureList::IsEnabled(features::kGlicActor) &&
          base::FeatureList::IsEnabled(features::kGlicActorUi) &&
          features::kGlicActorUiTaskIcon.Get() &&
          browser_->GetProfile()->IsRegularProfile()) {
        // Will be referenced in GlicActorNudgeController and thus needs to be
        // instantiated first.
        actor_task_list_bubble_controller_ =
            GetUserDataFactory().CreateInstance<ActorTaskListBubbleController>(
                *browser_, browser_);
        // Includes browser twice to enable injecting for testing.
        glic_actor_nudge_controller_ =
            GetUserDataFactory().CreateInstance<glic::GlicActorNudgeController>(
                *browser_, browser_, tab_strip_container, toolbar_view);
      }
    }

    // Memory Saver mode is default off but is available to turn on.
    // The controller relies on performance manager which isn't initialized in
    // some unit tests without browser view.
    memory_saver_opt_in_iph_controller_ =
        GetUserDataFactory().CreateInstance<MemorySaverOptInIPHController>(
            *browser_, browser_);

    if (media_router::MediaRouterEnabled(browser_view->browser()->profile())) {
      cast_browser_controller_ =
          std::make_unique<media_router::CastBrowserController>(
              browser_view->browser());
    }

    if (base::FeatureList::IsEnabled(features::kGlicActorUi)) {
      std::vector<std::pair<views::WebView*, ActorOverlayWebView*>>
          container_overlay_view_pairs;
      for (auto* contents_container :
           browser_view->GetContentsContainerViews()) {
        container_overlay_view_pairs.emplace_back(
            contents_container->contents_view(),
            contents_container->actor_overlay_web_view());
      }
      actor_ui_window_controller_ =
          GetUserDataFactory().CreateInstance<ActorUiWindowController>(
              *browser_, browser_, std::move(container_overlay_view_pairs));
    }

    if (features::HasTabSearchToolbarButton() ||
        tabs::IsVerticalTabsFeatureEnabled()) {
      tab_search_toolbar_button_controller_ =
          GetUserDataFactory().CreateInstance<TabSearchToolbarButtonController>(
              *browser_, browser_.get(), browser_view);
    }
  }

  if (browser_->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL ||
      browser_->GetType() == BrowserWindowInterface::Type::TYPE_POPUP ||
      browser_view->GetIsWebAppType()) {
    data_protection_ui_controller_ =
        GetUserDataFactory()
            .CreateInstance<
                enterprise_data_protection::DataProtectionUIController>(
                *browser_view->browser(), browser_view);
  }

#if !BUILDFLAG(IS_CHROMEOS)
  download_toolbar_ui_controller_ =
      GetUserDataFactory().CreateInstance<DownloadToolbarUIController>(
          *browser_view->browser(), browser_view);
#endif

  if (base::FeatureList::IsEnabled(ntp_features::kNtpFooter)) {
    new_tab_footer_controller_ =
        std::make_unique<new_tab_footer::NewTabFooterController>(
            browser_view->browser()->GetProfile(),
            browser_view->GetContentsContainerViews());
  }

  devtools_ui_controller_ = std::make_unique<DevtoolsUIController>(
      browser_, browser_view->GetContentsContainerViews());

#if BUILDFLAG(IS_WIN)
  windows_taskbar_icon_updater_ =
      std::make_unique<WindowsTaskbarIconUpdater>(*browser_view);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS) && (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC))
  if (base::FeatureList::IsEnabled(
          extensions_features::kSearchEngineExplicitChoiceDialog)) {
    default_search_extension_controlled_controller_ =
        GetUserDataFactory()
            .CreateInstance<DefaultSearchExtensionControlledController>(
                *browser_, *browser_, *browser_->GetProfile());
  }
#endif

  zoom_bubble_manager_ = std::make_unique<ZoomBubbleManagerViews>(browser_view);
  zoom_bubble_coordinator_ =
      GetUserDataFactory().CreateInstance<ZoomBubbleCoordinator>(
          *browser_, *browser_, zoom_bubble_manager_.get());

  user_education_->Init(browser_view);

  find_bar_owner_ = std::make_unique<FindBarOwnerViews>(browser_view);

  omnibox_popup_closer_ =
      std::make_unique<omnibox::OmniboxPopupCloser>(browser_view);

  skills_ui_window_controller_ =
      std::make_unique<skills::SkillsUiWindowController>(browser_);

  // Initialize post-BrowserView-dependent embedder features last.
  embedder_browser_window_features_->InitPostBrowserViewConstruction(
      browser_view);
}

void BrowserWindowFeatures::TearDownPreBrowserWindowDestruction() {
  // Tear down embedder features first, in reverse order of initialization.
  embedder_browser_window_features_->TearDownPreBrowserWindowDestruction();

  accelerator_provider_ = nullptr;
  extension_keybinding_registry_.reset();
  contents_border_controller_.reset();
  live_tab_context_.reset();
  upgrade_notification_controller_.reset();
  memory_saver_opt_in_iph_controller_.reset();
  ai_overlay_dialog_controller_.reset();
  lens_overlay_entry_point_controller_.reset();
  initial_web_ui_manager_.reset();
  tab_search_toolbar_button_controller_.reset();
  profile_menu_coordinator_.reset();
  toast_service_.reset();
  extension_window_controller_.reset();
  actor_border_view_controller_.reset();
  glic_button_controller_.reset();
  glic_actor_nudge_controller_.reset();
  actor_task_list_bubble_controller_.reset();

  contextual_tasks_close_button_controller_.reset();
  contextual_tasks_ephemeral_button_controller_.reset();
  contextual_tasks_side_panel_coordinator_.reset();
  contextual_tasks_entry_point_eligibility_manager_.reset();

#if !BUILDFLAG(IS_CHROMEOS)
  if (download_toolbar_ui_controller_) {
    download_toolbar_ui_controller_->TearDownPreBrowserWindowDestruction();
  }
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS) && (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC))
  default_search_extension_controlled_controller_.reset();
#endif

  zoom_bubble_coordinator_.reset();
  zoom_bubble_manager_.reset();

  comments_side_panel_coordinator_.reset();

  history_clusters_side_panel_coordinator_.reset();

  // TODO(crbug.com/346148093): This logic should not be gated behind a
  // conditional.
  if (side_panel_coordinator_) {
    side_panel_coordinator_->TearDownPreBrowserWindowDestruction();
  }


  color_provider_browser_helper_.reset();

  if (shared_tab_group_feedback_controller_) {
    shared_tab_group_feedback_controller_->TearDown();
  }

  if (chrome_labs_coordinator_) {
    chrome_labs_coordinator_->TearDown();
  }

  if (new_tab_footer_controller_) {
    new_tab_footer_controller_->TearDown();
  }

  if (devtools_ui_controller_) {
    devtools_ui_controller_->TearDown();
  }

  if (actor_ui_window_controller_) {
    actor_ui_window_controller_->TearDown();
  }

  data_protection_ui_controller_.reset();

  browser_focus_controller_.reset();
  desktop_browser_window_capabilities_.reset();
  signin_view_controller_->TearDownPreBrowserWindowDestruction();

  // Destroy fullscreen control host before exclusive access manager.
  fullscreen_control_host_.reset();

  pinned_toolbar_actions_ = nullptr;

  // TODO(crbug.com/423956131): Update reset order once FindBarController is
  // deterministically constructed.
  find_bar_controller_.reset();

  omnibox_popup_closer_.reset();

  split_tab_highlight_controller_.reset();

  extension_installed_watcher_.reset();

#if BUILDFLAG(IS_WIN)
  windows_taskbar_icon_updater_.reset();
#endif

  if (user_education_) {
    user_education_->TearDown();
  }

  bookmark_bar_controller_->SetDelegate(nullptr);

  immersive_mode_controller_.reset();

  exclusive_access_manager_.reset();

  webui_browser_exclusive_access_context_.reset();

  scrim_view_controller_.reset();

  ios_promo_controller_.reset();

  if (auto* const provider = browser_elements_->AsA<BrowserElementsViews>()) {
    provider->TearDown();
  }

  find_bar_owner_.reset();

  ai_mode_page_action_controller_.reset();

  skills_ui_window_controller_.reset();

  context_highlight_window_feature_.reset();

  browser_select_file_dialog_controller_.reset();

  browser_animation_controller_.reset();
}

SidePanelUI* BrowserWindowFeatures::side_panel_ui() {
  // TODO(crbug.com/428946261): Remove this and replace all clients with
  // `SidePanelUI::From()`.
  return browser_ ? SidePanelUI::From(browser_) : nullptr;
}

actions::ActionItem* BrowserWindowFeatures::GetRootActionItem() {
  return browser_actions() ? browser_actions()->root_action_item() : nullptr;
}

ToastController* BrowserWindowFeatures::toast_controller() {
  return toast_service_ ? toast_service_->toast_controller() : nullptr;
}

LocationBar* BrowserWindowFeatures::location_bar() {
  // Return nullptr if not initialized. This can happen in tests where
  // BrowserWindowFeatures is stubbed without being initialized with a browser.
  if (!browser_) {
    return nullptr;
  }
  return browser_->GetBrowserForMigrationOnly()->window()->GetLocationBar();
}

const LocationBar* BrowserWindowFeatures::location_bar() const {
  // Return nullptr if not initialized. This can happen in tests where
  // BrowserWindowFeatures is stubbed without being initialized with a browser.
  if (!browser_) {
    return nullptr;
  }
  return browser_->GetBrowserForMigrationOnly()->window()->GetLocationBar();
}

FindBarController* BrowserWindowFeatures::GetFindBarController() {
  if (!find_bar_controller_.get()) {
    CHECK(browser_);
    find_bar_controller_ = std::make_unique<FindBarController>(
        browser_->GetBrowserForMigrationOnly()->window()->CreateFindBar());
    find_bar_controller_->find_bar()->SetFindBarController(
        find_bar_controller_.get());
    find_bar_controller_->ChangeWebContents(
        tab_strip_model_->GetActiveWebContents());
    find_bar_controller_->find_bar()->MoveWindowIfNecessary();
  }
  return find_bar_controller_.get();
}

bool BrowserWindowFeatures::HasFindBarController() const {
  return find_bar_controller_.get() != nullptr;
}

// static
ui::UserDataFactoryWithOwner<BrowserWindowInterface>&
BrowserWindowFeatures::GetUserDataFactoryForTesting() {
  return GetUserDataFactory();
}

// static
ui::UserDataFactoryWithOwner<BrowserWindowInterface>&
BrowserWindowFeatures::GetUserDataFactory() {
  static base::NoDestructor<
      ui::UserDataFactoryWithOwner<BrowserWindowInterface>>
      factory;
  return *factory;
}
