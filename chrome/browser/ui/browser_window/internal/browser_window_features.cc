// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/browser_window_features.h"

#include <memory>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chrome/browser/actor/ui/actor_overlay_window_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/collaboration/collaboration_service_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"
#include "chrome/browser/extensions/mv2_experiment_stage.h"
#include "chrome/browser/lens/region_search/lens_region_search_controller.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar_controller.h"
#include "chrome/browser/ui/breadcrumb_manager_browser_agent.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "chrome/browser/ui/browser_location_bar_model_delegate.h"
#include "chrome/browser/ui/browser_tab_menu_model_delegate.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"
#include "chrome/browser/ui/commerce/product_specifications_entry_point_controller.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/extensions/mv2_disabled_dialog_controller.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_entry_point_controller.h"
#include "chrome/browser/ui/performance_controls/memory_saver_bubble_controller.h"
#include "chrome/browser/ui/performance_controls/memory_saver_opt_in_iph_controller.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/sync/browser_synced_window_delegate.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/glic_actor_task_icon_controller.h"
#include "chrome/browser/ui/tabs/glic_nudge_controller.h"
#include "chrome/browser/ui/tabs/organization/tab_declutter_controller.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/most_recent_shared_tab_update_store.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/session_service_tab_group_sync_observer.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/shared_tab_group_feedback_controller.h"
#include "chrome/browser/ui/tabs/split_tab_scrim_controller.h"
#include "chrome/browser/ui/tabs/tab_group_deletion_dialog_controller.h"
#include "chrome/browser/ui/tabs/tab_list_bridge.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_impl.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/browser/ui/toasts/toast_service.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_utils.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/tab_search_toolbar_button_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/color_provider_browser_helper.h"
#include "chrome/browser/ui/views/data_sharing/data_sharing_bubble_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/incognito_clear_browsing_data_dialog_coordinator.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_coordinator.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/media_router/cast_browser_controller.h"
#include "chrome/browser/ui/views/new_tab_footer/footer_controller.h"
#include "chrome/browser/ui/views/profiles/profile_menu_coordinator.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_bubble_controller.h"
#include "chrome/browser/ui/views/side_panel/bookmarks/bookmarks_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/comments/comments_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_manager.h"
#include "chrome/browser/ui/views/side_panel/history/history_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/history_clusters/history_clusters_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/reading_list/reading_list_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_coordinator.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_controller.h"
#include "chrome/browser/ui/views/translate/translate_bubble_controller.h"
#include "chrome/browser/ui/views/upgrade_notification_controller.h"
#include "chrome/browser/ui/views/user_education/impl/browser_user_education_interface_impl.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/chrome_features.h"
#include "components/breadcrumbs/core/breadcrumbs_status.h"
#include "components/collaboration/public/collaboration_service.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/feature_utils.h"
#include "components/commerce/core/shopping_service.h"
#include "components/lens/lens_features.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/omnibox/browser/location_bar_model_impl.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/search/ntp_features.h"
#include "components/search/search.h"
#include "content/public/common/content_constants.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_browser_window_helper.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/pdf/infobar/pdf_infobar_controller.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/startup/default_browser_prompt/pin_infobar/pin_infobar_controller.h"
#include "chrome/browser/ui/views/frame/windows_taskbar_icon_updater.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/bubble/download_display_controller.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_ui_controller.h"
#endif

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/browser_ui/glic_button_controller.h"
#include "chrome/browser/glic/browser_ui/glic_iph_controller.h"
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
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

  browser_actions_ = std::make_unique<BrowserActions>(browser);

  browser_command_controller_ =
      std::make_unique<chrome::BrowserCommandController>(browser);

  browser_actions_->InitializeBrowserActions();

  // Initialize bookmark bar controller for all browser types.
  bookmark_bar_controller_ = std::make_unique<BookmarkBarController>(
      *browser, *browser->GetTabStripModel());

  // Avoid passing `browser` directly to features. Instead, pass the minimum
  // necessary state or controllers necessary.
  // Ping erikchen for assistance. This comment will be deleted after there are
  // 10+ features.
  //
  // Features that are only enabled for normal browser windows (e.g. a window
  // with an omnibox and a tab strip). By default most features should be
  // instantiated in this block.
  if (browser->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL) {
    if (search::IsInstantExtendedAPIEnabled()) {
      instant_controller_ = std::make_unique<BrowserInstantController>(
          browser->GetProfile(), browser->GetTabStripModel());
    }

    if (browser->GetProfile()->IsRegularProfile()) {
      auto* shopping_service =
          commerce::ShoppingServiceFactory::GetForBrowserContext(
              browser->GetProfile());
      if (shopping_service && commerce::CanLoadProductSpecificationsFullPageUi(
                                  shopping_service->GetAccountChecker())) {
        product_specifications_entry_point_controller_ =
            GetUserDataFactory()
                .CreateInstance<
                    commerce::ProductSpecificationsEntryPointController>(
                    *browser, browser);
      }
    }

    if (browser->GetProfile()->IsRegularProfile() &&
        browser->GetTabStripModel()->SupportsTabGroups() &&
        tab_groups::SavedTabGroupUtils::GetServiceForProfile(
            browser->GetProfile())) {
      session_service_tab_group_sync_observer_ =
          std::make_unique<tab_groups::SessionServiceTabGroupSyncObserver>(
              browser->GetProfile(), browser->GetTabStripModel(),
              browser->GetSessionID());

      most_recent_shared_tab_update_store_ =
          std::make_unique<tab_groups::MostRecentSharedTabUpdateStore>(browser);
    }

    if (features::IsTabstripDeclutterEnabled() &&
        (browser->GetProfile()->IsRegularProfile() ||
         browser->GetProfile()->IsGuestSession())) {
      tab_declutter_controller_ =
          std::make_unique<tabs::TabDeclutterController>(browser);
    }

#if BUILDFLAG(ENABLE_GLIC)
    if (glic::GlicEnabling::IsProfileEligible(browser->GetProfile())) {
      DCHECK(features::HasTabSearchToolbarButton());
      glic_iph_controller_ = std::make_unique<glic::GlicIphController>(browser);
      glic_nudge_controller_ =
          std::make_unique<tabs::GlicNudgeController>(browser);
    }
#endif  // BUILDFLAG(ENABLE_GLIC)

    if (tabs::AreVerticalTabsEnabled()) {
      vertical_tab_strip_state_controller_ =
          std::make_unique<tabs::VerticalTabStripStateController>(
              browser->GetProfile()->GetPrefs());
    }
  }

  // The LensOverlayEntryPointController is constructed for all browser types
  // but is only initialized for normal browser windows. This simplifies the
  // logic for code shared by both normal and non-normal windows.
  lens_overlay_entry_point_controller_ =
      std::make_unique<lens::LensOverlayEntryPointController>();
  lens_region_search_controller_ =
      std::make_unique<lens::LensRegionSearchController>();

  tab_strip_model_ = browser->GetTabStripModel();

  if (base::FeatureList::IsEnabled(features::kTabStripBrowserApi)) {
    tab_strip_service_ =
        std::make_unique<TabStripServiceImpl>(browser, tab_strip_model_);
  }

  memory_saver_bubble_controller_ =
      std::make_unique<memory_saver::MemorySaverBubbleController>(browser);

  translate_bubble_controller_ = std::make_unique<TranslateBubbleController>(
      browser->GetActions()->root_action_item());

  cookie_controls_bubble_coordinator_ =
      std::make_unique<CookieControlsBubbleCoordinator>();

  tab_menu_model_delegate_ =
      std::make_unique<chrome::BrowserTabMenuModelDelegate>(
          browser->GetSessionID(), browser->GetProfile(),
          browser->GetAppBrowserController());

  tab_group_deletion_dialog_controller_ =
      std::make_unique<tab_groups::DeletionDialogController>(
          browser, browser->GetProfile(), tab_strip_model_);

  user_education_ =
      GetUserDataFactory().CreateInstance<BrowserUserEducationInterfaceImpl>(
          *browser, browser);

  location_bar_model_delegate_ =
      std::make_unique<BrowserLocationBarModelDelegate>(tab_strip_model_);
  location_bar_model_ = std::make_unique<LocationBarModelImpl>(
      location_bar_model_delegate_.get(), content::kMaxURLDisplayChars);

  reading_list_side_panel_coordinator_ =
      std::make_unique<ReadingListSidePanelCoordinator>(
          browser->GetProfile(), browser->GetTabStripModel());

  signin_view_controller_ = std::make_unique<SigninViewController>(
      browser, browser->GetProfile(), tab_strip_model_);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  if (base::FeatureList::IsEnabled(features::kPdfInfoBar)) {
    pdf_infobar_controller_ =
        std::make_unique<pdf::infobar::PdfInfoBarController>(browser);
  }
#endif

#if BUILDFLAG(IS_WIN)
  if (base::FeatureList::IsEnabled(features::kOfferPinToTaskbarInfoBar)) {
    pin_infobar_controller_ =
        std::make_unique<default_browser::PinInfoBarController>(browser);
  }
#endif

  data_sharing_bubble_controller_ =
      std::make_unique<DataSharingBubbleController>(
          browser, browser->GetProfile(), tab_strip_model_);

  content_setting_bubble_model_delegate_ =
      std::make_unique<BrowserContentSettingBubbleModelDelegate>(browser);

  tab_list_bridge_ = std::make_unique<TabListBridge>(
      *tab_strip_model_, browser->GetUnownedUserDataHost());

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extension_browser_window_helper_ =
      std::make_unique<extensions::ExtensionBrowserWindowHelper>(
          browser_command_controller_.get(), browser->GetTabStripModel(),
          browser->GetProfile());
#endif

  if (breadcrumbs::IsEnabled(g_browser_process->local_state())) {
    breadcrumb_manager_browser_agent_ =
        std::make_unique<BreadcrumbManagerBrowserAgent>(
            browser->GetTabStripModel(), browser->GetProfile());
  }

#if defined(USE_AURA)
  overscroll_pref_manager_ = std::make_unique<OverscrollPrefManager>(
      tab_strip_model_,
      browser->GetType() == BrowserWindowInterface::Type::TYPE_DEVTOOLS);
#endif  // defined(USE_AURA)
}

void BrowserWindowFeatures::InitPostWindowConstruction(Browser* browser) {
  desktop_browser_window_capabilities_ =
      GetUserDataFactory().CreateInstance<DesktopBrowserWindowCapabilities>(
          *browser, browser, browser->window(),
          browser->GetUnownedUserDataHost());

  exclusive_access_manager_ = std::make_unique<ExclusiveAccessManager>(
      browser->window()->GetExclusiveAccessContext());

  // This code needs exclusive access manager to be initialized.
#if !BUILDFLAG(IS_CHROMEOS)
  if (download_toolbar_ui_controller_) {
    download_toolbar_ui_controller_->display_controller()
        ->ListenToFullScreenChanges();
  }
#endif

  // Features that are only enabled for normal browser windows (e.g. a window
  // with an omnibox and a tab strip). By default most features should be
  // instantiated in this block.
  if (browser->is_type_normal()) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);

    if (IsChromeLabsEnabled()) {
      chrome_labs_coordinator_ =
          std::make_unique<ChromeLabsCoordinator>(browser);
    }

    send_tab_to_self_toolbar_bubble_controller_ = std::make_unique<
        send_tab_to_self::SendTabToSelfToolbarBubbleController>(browser);

    if (browser_view) {
      // The controller should only be created if the
      // PinnedToolbarActionsContainer exists for the browser, this might not be
      // the case for browsers with a custom tab toolbar.
      if (auto* pinned_toolbar_actions_container =
              browser_view->toolbar_button_provider()
                  ->GetPinnedToolbarActionsContainer()) {
        pinned_toolbar_actions_controller_ =
            std::make_unique<PinnedToolbarActionsController>(
                pinned_toolbar_actions_container);
      }
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

    auto* experiment_manager =
        extensions::ManifestV2ExperimentManager::Get(browser->profile());
    if (experiment_manager) {
      extensions::MV2ExperimentStage experiment_stage =
          experiment_manager->GetCurrentExperimentStage();
      if (experiment_stage ==
              extensions::MV2ExperimentStage::kDisableWithReEnable ||
          experiment_stage == extensions::MV2ExperimentStage::kUnsupported) {
        mv2_disabled_dialog_controller_ =
            std::make_unique<extensions::Mv2DisabledDialogController>(browser);
      }
    }

    if (features::HasTabSearchToolbarButton()) {
      // TODO(crbug.com/360163254): We should really be using
      // Browser::GetBrowserView, which always returns a non-null BrowserView
      // in production, but this crashes during unittests using
      // BrowserWithTestWindowTest; these should eventually be refactored.
      if (browser_view) {
        tab_search_toolbar_button_controller_ =
            std::make_unique<TabSearchToolbarButtonController>(
                browser_view, browser_view->GetTabSearchBubbleHost());
      }
    }

    if (browser->GetTabStripModel()->SupportsTabGroups() &&
        tab_groups::SavedTabGroupUtils::SupportsSharedTabGroups() &&
        tab_groups::SavedTabGroupUtils::GetServiceForProfile(
            browser->GetProfile())) {
      if (browser_view) {
        shared_tab_group_feedback_controller_ =
            std::make_unique<tab_groups::SharedTabGroupFeedbackController>(
                browser_view->browser());
        shared_tab_group_feedback_controller_->Init();
      }
    }

    if (base::FeatureList::IsEnabled(features::kSideBySide)) {
      if (browser_view) {
        split_tab_scrim_controller_ =
            std::make_unique<split_tabs::SplitTabScrimController>(browser_view);
      }
    }
  }

  synced_window_delegate_ = std::make_unique<BrowserSyncedWindowDelegate>(
      browser, browser->GetTabStripModel(), browser->GetSessionID(),
      browser->GetType());

  extension_window_controller_ =
      std::make_unique<extensions::BrowserExtensionWindowController>(browser);

  profile_menu_coordinator_ =
      std::make_unique<ProfileMenuCoordinator>(browser, browser->GetProfile());

  upgrade_notification_controller_ =
      std::make_unique<UpgradeNotificationController>(browser);

  incognito_clear_browsing_data_dialog_coordinator_ =
      std::make_unique<IncognitoClearBrowsingDataDialogCoordinator>(
          browser->GetProfile());

  if (auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser)) {
    color_provider_browser_helper_ =
        std::make_unique<ColorProviderBrowserHelper>(
            browser->GetTabStripModel(), browser_view->GetWidget());
  }

  live_tab_context_ = std::make_unique<BrowserLiveTabContext>(
      browser, browser->GetTabStripModel(), browser->GetProfile(),
      browser->GetWindow(), browser->GetType(), browser->app_name(),
      browser->GetSessionID());

  if (browser->is_type_normal() || browser->is_type_app()) {
    toast_service_ = std::make_unique<ToastService>(browser);
  }
}

void BrowserWindowFeatures::InitPostBrowserViewConstruction(
    BrowserView* browser_view) {
  // TODO(crbug.com/346148093): Move SidePanelCoordinator construction to Init.
  // TODO(crbug.com/346148554): Do not create a SidePanelCoordinator for most
  // browser.h types
  // Conceptually, SidePanelCoordinator handles the "model" whereas
  // BrowserView::unified_side_panel_ handles the "ui". When we stop making this
  // for most browser.h types, we should also stop making the
  // unified_side_panel_.
  side_panel_coordinator_ =
      std::make_unique<SidePanelCoordinator>(browser_view);

  if (HistorySidePanelCoordinator::IsSupported()) {
    history_side_panel_coordinator_ =
        std::make_unique<HistorySidePanelCoordinator>(browser_view->browser());
  }

  history_clusters_side_panel_coordinator_ =
      std::make_unique<HistoryClustersSidePanelCoordinator>(
          browser_, browser_->GetProfile(), side_panel_coordinator_.get());

  bookmarks_side_panel_coordinator_ =
      std::make_unique<BookmarksSidePanelCoordinator>();

  if (CommentsSidePanelCoordinator::IsSupported()) {
    comments_side_panel_coordinator_ =
        std::make_unique<CommentsSidePanelCoordinator>(browser_view->browser());
  }

  side_panel_coordinator_->Init(browser_view->browser());

  extension_side_panel_manager_ =
      std::make_unique<extensions::ExtensionSidePanelManager>(
          browser_view->browser(),
          side_panel_coordinator_->GetWindowRegistry());

  immersive_mode_controller_ =
      chrome::CreateImmersiveModeController(browser_view);

  // Memory Saver mode is default off but is available to turn on.
  // The controller relies on performance manager which isn't initialized in
  // some unit tests without browser view.
  if (browser_view->GetIsNormalType()) {
#if BUILDFLAG(ENABLE_GLIC)
    glic::GlicKeyedService* glic_service =
        glic::GlicKeyedService::Get(browser_view->GetProfile());
    if (glic_service) {
      glic_button_controller_ = std::make_unique<glic::GlicButtonController>(
          browser_view->GetProfile(),
          browser_view->tab_strip_region_view()->GetTabStripActionContainer(),
          glic_service);

      if (features::kGlicActorUiTaskIcon.Get() &&
          browser_->GetProfile()->IsRegularProfile()) {
        glic_actor_task_icon_controller_ =
            std::make_unique<tabs::GlicActorTaskIconController>(
                browser_->GetProfile(), browser_view->tab_strip_region_view()
                                            ->GetTabStripActionContainer());
      }
    }

#endif  // BUILDFLAG(ENABLE_GLIC)

    memory_saver_opt_in_iph_controller_ =
        std::make_unique<MemorySaverOptInIPHController>(
            browser_view->browser());

    if (media_router::MediaRouterEnabled(browser_view->browser()->profile())) {
      cast_browser_controller_ =
          std::make_unique<media_router::CastBrowserController>(
              browser_view->browser());
    }

    if (features::kGlicActorUiOverlay.Get()) {
      // TODO(crbug.com/433999185): Handle split view.
      actor_overlay_window_controller_ =
          std::make_unique<actor::ui::ActorOverlayWindowController>(
              browser_view->GetActiveContentsContainerView()
                  ->GetActorOverlayView());
    }
  }

#if !BUILDFLAG(IS_CHROMEOS)
  download_toolbar_ui_controller_ =
      std::make_unique<DownloadToolbarUIController>(browser_view);
#endif

  if (base::FeatureList::IsEnabled(ntp_features::kNtpFooter)) {
    new_tab_footer_controller_ =
        std::make_unique<new_tab_footer::NewTabFooterController>(
            browser_view->browser()->GetProfile(),
            browser_view->GetContentsContainerViews());
  }

#if BUILDFLAG(IS_WIN)
  windows_taskbar_icon_updater_ =
      std::make_unique<WindowsTaskbarIconUpdater>(*browser_view);
#endif

  user_education_->Init(browser_view);
}

void BrowserWindowFeatures::TearDownPreBrowserWindowDestruction() {
  live_tab_context_.reset();
  upgrade_notification_controller_.reset();
  memory_saver_opt_in_iph_controller_.reset();
  lens_overlay_entry_point_controller_.reset();
  tab_search_toolbar_button_controller_.reset();
  profile_menu_coordinator_.reset();
  toast_service_.reset();
  extension_window_controller_.reset();
  actor_overlay_window_controller_.reset();
  glic_actor_task_icon_controller_.reset();

#if BUILDFLAG(ENABLE_GLIC)
  glic_button_controller_.reset();
#endif

#if !BUILDFLAG(IS_CHROMEOS)
  if (download_toolbar_ui_controller_) {
    download_toolbar_ui_controller_->TearDownPreBrowserWindowDestruction();
  }
#endif

  comments_side_panel_coordinator_.reset();

  history_clusters_side_panel_coordinator_.reset();

  // TODO(crbug.com/346148093): This logic should not be gated behind a
  // conditional.
  if (side_panel_coordinator_) {
    side_panel_coordinator_->TearDownPreBrowserWindowDestruction();
  }

  if (mv2_disabled_dialog_controller_) {
    mv2_disabled_dialog_controller_->TearDown();
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

  desktop_browser_window_capabilities_.reset();
  signin_view_controller_->TearDownPreBrowserWindowDestruction();

  if (pinned_toolbar_actions_controller_) {
    pinned_toolbar_actions_controller_->TearDown();
  }

  // TODO(crbug.com/423956131): Update reset order once FindBarController is
  // deterministically constructed.
  find_bar_controller_.reset();

  split_tab_scrim_controller_.reset();

#if BUILDFLAG(IS_WIN)
  windows_taskbar_icon_updater_.reset();
#endif

  if (user_education_) {
    user_education_->TearDown();
  }

  immersive_mode_controller_.reset();

  exclusive_access_manager_.reset();
}

SidePanelUI* BrowserWindowFeatures::side_panel_ui() {
  return side_panel_coordinator_.get();
}

ToastController* BrowserWindowFeatures::toast_controller() {
  return toast_service_ ? toast_service_->toast_controller() : nullptr;
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
