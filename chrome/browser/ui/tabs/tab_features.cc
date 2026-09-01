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
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_tab_data.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"
#include "chrome/browser/banners/app_banner_manager_desktop.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/commerce/in_stock_notification/in_stock_notification_manager.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_controller.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_web_contents_observer.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/enterprise/data_protection/data_protection_navigation_controller.h"
#include "chrome/browser/enterprise/reporting/saas_usage/saas_usage_navigation_observer.h"
#include "chrome/browser/geic/geic_enabling.h"
#include "chrome/browser/geic/geic_side_panel_coordinator.h"
#include "chrome/browser/glic/host/context/glic_page_features_manager.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_helper.h"
#include "chrome/browser/glic/suggestions/glic_cue_tab_state.h"
#include "chrome/browser/glic/suggestions/glic_cue_target.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/indigo/indigo_cue_target.h"
#include "chrome/browser/indigo/indigo_page_action_controller.h"
#include "chrome/browser/loader/from_gws_navigation_and_keep_alive_request_observer.h"
#include "chrome/browser/multistep_filter/chrome_filter_navigation_observer.h"
#include "chrome/browser/multistep_filter/ui/filter_ui_controller.h"
#include "chrome/browser/net/http_auth_cache_status.h"
#include "chrome/browser/net/qwac_web_contents_observer.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/payments/web_payments_observer.h"
#include "chrome/browser/preloading/bookmarkbar_preload/bookmarkbar_preload_pipeline_manager.h"
#include "chrome/browser/preloading/new_tab_page_preload/new_tab_page_preload_pipeline_manager.h"
#include "chrome/browser/preloading/prefetch/zero_suggest_prefetch/zero_suggest_prefetch_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ssl/ask_before_http_dialog_controller.h"
#include "chrome/browser/ssl/connection_help_tab_helper.h"
#include "chrome/browser/ssl/security_state_event_observer.h"
#include "chrome/browser/sync/sessions/sync_sessions_router_tab_helper.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/tab_contents/form_interaction_tab_helper.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/autofill/bubble_manager.h"
#include "chrome/browser/ui/autofill/payments/omnibox_autofill_bubble_controller.h"
#include "chrome/browser/ui/autofill/payments/omnibox_autofill_page_action_controller.h"
#include "chrome/browser/ui/autofill/payments/payments_churned_users_bubble_controller.h"
#include "chrome/browser/ui/autofill/payments/payments_churned_users_page_action_controller.h"
#include "chrome/browser/ui/autofill/payments/wallet_reminder_notice_bubble_controller.h"
#include "chrome/browser/ui/autofill/payments/wallet_reminder_notice_page_action_controller.h"
#include "chrome/browser/ui/blocked_content/framebust_block_tab_helper.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/commerce/commerce_ui_tab_helper.h"
#include "chrome/browser/ui/context_highlight/context_highlight_tab_feature.h"
#include "chrome/browser/ui/extensions/extension_side_panel_manager.h"
#include "chrome/browser/ui/focus_tab_after_navigation_helper.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/page_action/action_ids.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/page_action/page_action_properties_provider.h"
#include "chrome/browser/ui/performance_controls/memory_saver_chip_controller.h"
#include "chrome/browser/ui/performance_controls/memory_saver_chip_tab_helper.h"
#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"
#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/search_engine_choice/search_engine_choice_tab_helper.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/sync/browser_synced_tab_delegate.h"
#include "chrome/browser/ui/tab_ui_helper.h"
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
#include "chrome/browser/ui/views/translate/translate_page_action_controller.h"
#include "chrome/browser/ui/views/zoom/zoom_view_controller.h"
#include "chrome/browser/ui/web_applications/pwa_install_page_action.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/ui/webui_browser/webui_browser.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/contextual_tasks/public/features.h"
#include "components/enterprise/browser/reporting/reporting_features.h"
#include "components/multistep_filter/core/features.h"
#include "components/payments/core/features.h"
#include "components/skills/features.h"
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/contextual_tasks/contextual_tasks_tab_visit_tracker.h"
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
#include "chrome/browser/glic/glic_selection_observer.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/widget/glic_side_panel_coordinator_impl.h"
#include "chrome/browser/glic/selection/selection_overlay_controller.h"
#include "chrome/browser/glic/service/glic_instance_helper.h"
#include "chrome/browser/skills/skills_ui_tab_controller.h"
#include "chrome/browser/skills/skills_update_observer.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_attachment_tracker.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/passage_embeddings/core/passage_embeddings_features.h"
#include "components/permissions/permission_indicators_tab_data.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/security_interstitials/core/features.h"
#include "components/tabs/public/tab_interface.h"
#include "components/wallet/core/common/wallet_features.h"
#include "net/base/features.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/unowned_user_data/user_data_factory.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"  // nogncheck
#include "chrome/browser/ui/views/web_apps/protocol_handler_picker_coordinator.h"
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
        std::make_unique<memory_saver::MemorySaverChipController>(
            *page_action_controller_);
  }

  if (page_action_controller_->ActionExists(kActionShowIntentPicker)) {
    intent_picker_view_page_action_controller_ =
        std::make_unique<IntentPickerViewPageActionController>(tab);
  }

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
      glic_selection_overlay_controller_ =
          GetUserDataFactory().CreateInstance<glic::SelectionOverlayController>(
              tab, &tab, profile->GetPrefs());

      if (glic::GlicEnabling::IsSelectionPromptEnabledForProfile(profile)) {
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
    if (geic::IsGeicEnabled(profile)) {
      geic_side_panel_coordinator_ =
          GetUserDataFactory().CreateInstance<geic::GeicSidePanelCoordinator>(
              tab, tab, side_panel_registry_.get());
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

  customize_chrome_side_panel_controller_ =
      std::make_unique<customize_chrome::SidePanelControllerViews>(tab);

  extension_side_panel_manager_ =
      std::make_unique<extensions::ExtensionSidePanelManager>(
          profile, &tab, side_panel_registry_.get());

  tab_dialog_manager_ = std::make_unique<TabDialogManager>(&tab);

  data_protection_tab_controller_ = std::make_unique<
      enterprise_data_protection::DataProtectionNavigationController>(&tab);

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
#endif

  if (base::FeatureList::IsEnabled(net::features::kVerifyQWACs)) {
    qwac_web_contents_observer_ =
        std::make_unique<QwacWebContentsObserver>(tab);
  }

  if (base::FeatureList::IsEnabled(contextual_cueing::kContextualCueingV2)) {
    contextual_cueing_controller_ =
        std::make_unique<contextual_cueing::ContextualCueingController>(&tab);
    glic::GlicCueTarget::Register(tab);
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
