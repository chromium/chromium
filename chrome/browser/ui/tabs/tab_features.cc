// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/public/tab_features.h"

#include <memory>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_tab_data.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browsing_topics/browsing_topics_service_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_helper.h"
#include "chrome/browser/enterprise/data_protection/data_protection_navigation_controller.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/loader/from_gws_navigation_and_keep_alive_request_observer.h"
#include "chrome/browser/net/http_auth_cache_status.h"
#include "chrome/browser/net/qwac_web_contents_observer.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/preloading/bookmarkbar_preload/bookmarkbar_preload_pipeline_manager.h"
#include "chrome/browser/preloading/new_tab_page_preload/new_tab_page_preload_pipeline_manager.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_tab_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ssl/ask_before_http_dialog_controller.h"
#include "chrome/browser/sync/sessions/sync_sessions_router_tab_helper.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/autofill/bubble_manager.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/commerce/commerce_ui_tab_helper.h"
#include "chrome/browser/ui/cookie_controls/roll_back_mode_b_infobar_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/performance_controls/memory_saver_chip_controller.h"
#include "chrome/browser/ui/performance_controls/memory_saver_chip_tab_helper.h"
#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"
#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_side_panel_controller.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/inactive_window_mouse_event_controller.h"
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
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_translate_action_listener.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_page_action_controller.h"
#include "chrome/browser/ui/views/commerce/discounts_page_action_view_controller.h"
#include "chrome/browser/ui/views/commerce/price_insights_page_action_view_controller.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_page_action_controller.h"
#include "chrome/browser/ui/views/file_system_access/file_system_access_page_action_controller.h"
#include "chrome/browser/ui/views/intent_picker/intent_picker_view_page_action_controller.h"
#include "chrome/browser/ui/views/js_optimization/js_optimizations_page_action_controller.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_page_action_controller.h"
#include "chrome/browser/ui/views/location_bar/lens_overlay_homework_page_action_controller.h"
#include "chrome/browser/ui/views/page_action/action_ids.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_properties_provider.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_page_action_controller.h"
#include "chrome/browser/ui/views/side_panel/customize_chrome/side_panel_controller_views.h"
#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_manager.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/translate/translate_page_action_controller.h"
#include "chrome/browser/ui/views/zoom/zoom_view_controller.h"
#include "chrome/browser/ui/web_applications/pwa_install_page_action.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/contextual_tasks/public/features.h"
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/wallet/chrome_walletable_pass_client.h"
#endif
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/browsing_topics/browsing_topics_service.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/passage_embeddings/passage_embeddings_features.h"
#include "components/permissions/permission_indicators_tab_data.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/security_interstitials/core/features.h"
#include "components/tabs/public/tab_interface.h"
#include "components/wallet/core/common/wallet_features.h"
#include "net/base/features.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/unowned_user_data/user_data_factory.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/browser_ui/glic_tab_indicator_helper.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/service/glic_instance_helper.h"
#include "chrome/browser/ui/views/side_panel/glic/glic_side_panel_coordinator.h"

#endif

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
  side_panel_registry_ = std::make_unique<SidePanelRegistry>(&tab);

  // This block instantiate the page action controllers. They do not require any
  // pre-condition. Because some feature need them during their instantiation,
  // therefore this block should come before the feature controllers
  // instantiation.
  if (base::FeatureList::IsEnabled(features::kPageActionsMigration)) {
    auto* pinned_actions_model = PinnedToolbarActionsModel::Get(profile);
    CHECK(pinned_actions_model);
    auto page_action_controller =
        std::make_unique<page_actions::PageActionControllerImpl>(
            pinned_actions_model);
    page_action_controller->Initialize(
        tab,
        std::vector<actions::ActionId>(page_actions::kActionIds.begin(),
                                       page_actions::kActionIds.end()),
        page_actions::PageActionPropertiesProvider());
    page_action_controller_ = std::move(page_action_controller);

    if (IsPageActionMigrated(PageActionIconType::kTranslate)) {
      translate_page_action_controller_ =
          std::make_unique<TranslatePageActionController>(tab);
    }

    if (IsPageActionMigrated(PageActionIconType::kMemorySaver)) {
      memory_saver_chip_controller_ =
          std::make_unique<memory_saver::MemorySaverChipController>(
              *page_action_controller_);
    }

    if (IsPageActionMigrated(PageActionIconType::kIntentPicker)) {
      intent_picker_view_page_action_controller_ =
          std::make_unique<IntentPickerViewPageActionController>(tab);
    }

    if (IsPageActionMigrated(PageActionIconType::kFileSystemAccess)) {
      file_system_access_page_action_controller_ =
          std::make_unique<FileSystemAccessPageActionController>(tab);
    }

    if (IsPageActionMigrated(PageActionIconType::kZoom)) {
      zoom_view_controller_ = std::make_unique<zoom::ZoomViewController>(
          tab, *page_action_controller_);
    }

    if (IsPageActionMigrated(PageActionIconType::kPwaInstall)) {
      pwa_install_page_action_controller_ =
          std::make_unique<PwaInstallPageActionController>(
              tab, *page_action_controller_);
    }

    if (IsPageActionMigrated(PageActionIconType::kPriceInsights)) {
      commerce_price_insights_page_action_view_controller_ =
          GetUserDataFactory()
              .CreateInstance<commerce::PriceInsightsPageActionViewController>(
                  tab, tab, *page_action_controller_);
    }

    if (IsPageActionMigrated(PageActionIconType::kManagePasswords)) {
      manage_passwords_page_action_controller_ =
          std::make_unique<ManagePasswordsPageActionController>(
              *page_action_controller_);
    }

    if (IsPageActionMigrated(PageActionIconType::kCookieControls)) {
      cookie_controls_page_action_controller_ =
          GetUserDataFactory()
              .CreateInstance<CookieControlsPageActionController>(
                  tab, tab, *profile, *page_action_controller_);
      cookie_controls_page_action_controller_->Init();
    }

    if (IsPageActionMigrated(PageActionIconType::kLensOverlayHomework)) {
      lens_overlay_homework_page_action_controller_ =
          GetUserDataFactory()
              .CreateInstance<LensOverlayHomeworkPageActionController>(
                  tab, tab, *profile, *page_action_controller_);
    }

    if (base::FeatureList::IsEnabled(contextual_tasks::kContextualTasks) &&
        (contextual_tasks::kShowEntryPoint.Get() ==
         contextual_tasks::EntryPointOption::kPageActionRevisit)) {
      contextual_tasks_page_action_controller_ =
          GetUserDataFactory()
              .CreateInstance<ContextualTasksPageActionController>(tab, &tab);
    }

    if (IsPageActionMigrated(PageActionIconType::kBookmarkStar) &&
        tab.GetBrowserWindowInterface()->GetType() ==
            BrowserWindowInterface::TYPE_NORMAL) {
      bookmark_page_action_controller_ =
          GetUserDataFactory().CreateInstance<BookmarkPageActionController>(
              tab, tab, profile->GetPrefs(), *page_action_controller_);
    }

    js_optimizations_page_action_controller_ =
        std::make_unique<JsOptimizationsPageActionController>(
            tab, *page_action_controller_);
  }

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

    // Each time a new tab is created, validate the topics calculation schedule
    // to help investigate a scheduling bug (crbug.com/343750866).
    if (browsing_topics::BrowsingTopicsService* browsing_topics_service =
            browsing_topics::BrowsingTopicsServiceFactory::GetForProfile(
                profile)) {
      browsing_topics_service->ValidateCalculationSchedule();
    }

    permission_indicators_tab_data_ =
        std::make_unique<permissions::PermissionIndicatorsTabData>(
            tab.GetContents());

    pinned_translate_action_listener_ =
        std::make_unique<PinnedTranslateActionListener>(&tab);

    if (!profile->IsIncognitoProfile()) {
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

      if (base::FeatureList::IsEnabled(privacy_sandbox::kRollBackModeB)) {
        roll_back_mode_b_infobar_controller_ =
            std::make_unique<RollBackModeBInfoBarController>(tab.GetContents());
      }
    }

    contextual_cueing::ContextualCueingHelper::MaybeCreateForWebContents(
        tab.GetContents());

    privacy_sandbox_tab_observer_ =
        std::make_unique<privacy_sandbox::PrivacySandboxTabObserver>(
            tab.GetContents());

    if (tab_groups::TabGroupSyncService* tab_group_sync_service =
            tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile)) {
      saved_tab_group_web_contents_listener_ =
          std::make_unique<tab_groups::SavedTabGroupWebContentsListener>(
              tab_group_sync_service, &tab);

      if (features::IsTabGroupMenuMoreEntryPointsEnabled()) {
        saved_tab_group_on_close_helper_ =
            std::make_unique<tab_groups::SavedTabGroupOnCloseHelper>(
                tab_group_sync_service, &tab);
      }
    }

    if (tab_groups::SavedTabGroupUtils::SupportsSharedTabGroups()) {
      collaboration_messaging_tab_data_ =
          GetUserDataFactory()
              .CreateInstance<tab_groups::CollaborationMessagingTabData>(tab,
                                                                         &tab);
    }

    if (IsPageActionMigrated(PageActionIconType::kCollaborationMessaging) &&
        tab_groups::SavedTabGroupUtils::SupportsSharedTabGroups()) {
      collaboration_messaging_page_action_controller_ =
          GetUserDataFactory()
              .CreateInstance<CollaborationMessagingPageActionController>(
                  tab, tab, *page_action_controller_,
                  *collaboration_messaging_tab_data_);
    }

#if BUILDFLAG(ENABLE_GLIC)
    if (glic::GlicEnabling::IsProfileEligible(profile)) {
      glic_instance_helper_ =
          GetUserDataFactory().CreateInstance<glic::GlicInstanceHelper>(tab,
                                                                        &tab);
      glic_tab_indicator_helper_ =
          GetUserDataFactory().CreateInstance<glic::GlicTabIndicatorHelper>(
              tab, &tab);
    }
    if (glic::GlicEnabling::IsMultiInstanceEnabled() &&
        glic::GlicKeyedService::Get(profile)) {
      glic_side_panel_coordinator_ =
          GetUserDataFactory().CreateInstance<glic::GlicSidePanelCoordinator>(
              tab, &tab, side_panel_registry_.get());
    }
#endif  // BUILDFLAG(ENABLE_GLIC)
    // TODO(crbug.com/433973411): Move this logic to a helper function.
    if (base::FeatureList::IsEnabled(features::kGlicActorUi) &&
        profile->IsRegularProfile()) {
      // The associated tab is passed to CreateInstance twice: for dependency
      // injection callbacks and as a direct constructor argument.
      actor_ui_tab_controller_ =
          GetUserDataFactory().CreateInstance<actor::ui::ActorUiTabController>(
              tab, tab, actor::ActorKeyedService::Get(profile),
              std::make_unique<actor::ui::ActorUiTabControllerFactory>());
    }
    actor_tab_data_ =
        GetUserDataFactory().CreateInstance<actor::ActorTabData>(tab, &tab);
  }  // IsInNormalWindow() end.

  // This block instantiates the page action controllers that depends on the
  // `commerce_ui_tab_helper_` and not need to be created before.
  if (commerce_ui_tab_helper_) {
    if (IsPageActionMigrated(PageActionIconType::kDiscounts)) {
      commerce_discounts_page_action_view_controller_ =
          GetUserDataFactory()
              .CreateInstance<commerce::DiscountsPageActionViewController>(
                  tab, tab, *page_action_controller_, *commerce_ui_tab_helper_);
    }
  }

  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillShowBubblesBasedOnPriorities)) {
    autofill_bubble_manager_ = autofill::BubbleManager::Create(&tab);
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
  if (features::IsImmersiveReadAnythingEnabled()) {
    read_anything_controller_ =
        GetUserDataFactory().CreateInstance<ReadAnythingController>(tab, &tab);
  }

  // TODO(crbug.com/447418049): This will be removed in the future when
  // ownership of this controller is migrated to ReadAnythingController.
  read_anything_side_panel_controller_ =
      std::make_unique<ReadAnythingSidePanelController>(
          &tab, side_panel_registry_.get());

  // Create the HttpAuthCacheStatus to start observing resource load
  // completions.
  HttpAuthCacheStatus::HttpAuthCacheStatus::CreateForWebContents(
      tab.GetContents());

  if (web_app::AreWebAppsEnabled(profile)) {
    web_app::WebAppTabHelper::Create(&tab, tab.GetContents());
  }

  sync_sessions_router_ =
      std::make_unique<sync_sessions::SyncSessionsRouterTabHelper>(
          tab.GetContents(),
          sync_sessions::SyncSessionsWebContentsRouterFactory::GetForProfile(
              profile),
          ChromeTranslateClient::FromWebContents(tab.GetContents()),
          favicon::ContentFaviconDriver::FromWebContents(tab.GetContents()));

  from_gws_navigation_and_keep_alive_request_observer_ =
      FromGWSNavigationAndKeepAliveRequestObserver::MaybeCreateForWebContents(
          tab.GetContents());

  resource_usage_helper_ =
      GetUserDataFactory().CreateInstance<TabResourceUsageTabHelper>(tab, tab);

  memory_saver_chip_helper_ = std::make_unique<MemorySaverChipTabHelper>(tab);

  tab_creation_metrics_controller_ =
      std::make_unique<TabCreationMetricsController>(&tab);

  tab_ui_helper_ = std::make_unique<TabUIHelper>(tab);

  task_manager::WebContentsTags::CreateForTabContents(tab.GetContents());

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  inactive_window_mouse_event_controller_ =
      std::make_unique<InactiveWindowMouseEventController>();

  if (base::FeatureList::IsEnabled(wallet::kWalletablePassDetection)) {
    walletable_pass_client_ =
        std::make_unique<wallet::ChromeWalletablePassClient>(&tab);
  }
#endif

  if (base::FeatureList::IsEnabled(net::features::kVerifyQWACs)) {
    qwac_web_contents_observer_ =
        std::make_unique<QwacWebContentsObserver>(tab);
  }

  if (base::FeatureList::IsEnabled(
          security_interstitials::features::kHttpsFirstDialogUi)) {
    ask_before_http_dialog_controller_ =
        std::make_unique<AskBeforeHttpDialogController>(&tab);
  }

  bookmarkbar_preload_pipeline_manager_ =
      std::make_unique<BookmarkBarPreloadPipelineManager>(tab.GetContents());

  new_tab_page_preload_pipeline_manager_ =
      std::make_unique<NewTabPagePreloadPipelineManager>(tab.GetContents());

  tab_alert_controller_ =
      GetUserDataFactory().CreateInstance<TabAlertController>(tab, tab);

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

  // This method is transiently used to reset features that do not handle tab
  // discarding themselves.
  read_anything_side_panel_controller_->ResetForTabDiscard();
  read_anything_side_panel_controller_.reset();
  read_anything_side_panel_controller_ =
      std::make_unique<ReadAnythingSidePanelController>(
          tab, side_panel_registry_.get());

  // Deregister side-panel entries that are web-contents scoped rather than tab
  // scoped.
  side_panel_registry_->Deregister(
      SidePanelEntry::Key(SidePanelEntry::Id::kAboutThisSite));

  if (privacy_sandbox_tab_observer_) {
    privacy_sandbox_tab_observer_.reset();
    privacy_sandbox_tab_observer_ =
        std::make_unique<privacy_sandbox::PrivacySandboxTabObserver>(
            new_contents);
  }

  if (web_app::AreWebAppsEnabled(
          tab->GetBrowserWindowInterface()->GetProfile())) {
    web_app::WebAppTabHelper::Create(tab, new_contents);
  }

  sync_sessions_router_.reset();
  sync_sessions_router_ =
      std::make_unique<sync_sessions::SyncSessionsRouterTabHelper>(
          new_contents,
          sync_sessions::SyncSessionsWebContentsRouterFactory::GetForProfile(
              profile),
          ChromeTranslateClient::FromWebContents(new_contents),
          favicon::ContentFaviconDriver::FromWebContents(new_contents));

  if (permission_indicators_tab_data_) {
    permission_indicators_tab_data_ =
        std::make_unique<permissions::PermissionIndicatorsTabData>(
            new_contents);
  }

  if (roll_back_mode_b_infobar_controller_) {
    roll_back_mode_b_infobar_controller_.reset();
    roll_back_mode_b_infobar_controller_ =
        std::make_unique<RollBackModeBInfoBarController>(new_contents);
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
}

customize_chrome::SidePanelController*
TabFeatures::SetCustomizeChromeSidePanelControllerForTesting(
    std::unique_ptr<customize_chrome::SidePanelController>
        customize_chrome_side_panel_controller) {
  customize_chrome_side_panel_controller_ =
      std::move(customize_chrome_side_panel_controller);
  return customize_chrome_side_panel_controller_.get();
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
