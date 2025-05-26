// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/browser_window_features.h"

#include <memory>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chrome/browser/collaboration/collaboration_service_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/download/bubble/download_bubble_prefs.h"
#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"
#include "chrome/browser/extensions/mv2_experiment_stage.h"
#include "chrome/browser/lens/region_search/lens_region_search_controller.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/browser_tab_menu_model_delegate.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/commerce/product_specifications_entry_point_controller.h"
#include "chrome/browser/ui/extensions/mv2_disabled_dialog_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_entry_point_controller.h"
#include "chrome/browser/ui/performance_controls/memory_saver_bubble_controller.h"
#include "chrome/browser/ui/performance_controls/memory_saver_opt_in_iph_controller.h"
#include "chrome/browser/ui/tabs/glic_nudge_controller.h"
#include "chrome/browser/ui/tabs/organization/tab_declutter_controller.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/most_recent_shared_tab_update_store.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/session_service_tab_group_sync_observer.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/shared_tab_group_feedback_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_impl.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/browser/ui/toasts/toast_service.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_utils.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/tab_search_toolbar_button_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_ui_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_coordinator.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/media_router/cast_browser_controller.h"
#include "chrome/browser/ui/views/new_tab_footer/footer_controller.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_bubble_controller.h"
#include "chrome/browser/ui/views/side_panel/bookmarks/bookmarks_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_manager.h"
#include "chrome/browser/ui/views/side_panel/history/history_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_coordinator.h"
#include "chrome/browser/ui/views/translate/translate_bubble_controller.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/chrome_features.h"
#include "components/collaboration/public/collaboration_service.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/feature_utils.h"
#include "components/commerce/core/shopping_service.h"
#include "components/lens/lens_features.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/search/ntp_features.h"
#include "components/search/search.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/pdf/infobar/pdf_infobar_controller.h"
#endif

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/browser_ui/glic_button_controller.h"
#include "chrome/browser/glic/browser_ui/glic_iph_controller.h"
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#endif

namespace {

// This is the generic entry point for test code to stub out browser window
// functionality. It is called by production code, but only used by tests.
BrowserWindowFeatures::BrowserWindowFeaturesFactory& GetFactory() {
  static base::NoDestructor<BrowserWindowFeatures::BrowserWindowFeaturesFactory>
      factory;
  return *factory;
}

}  // namespace

// static
std::unique_ptr<BrowserWindowFeatures>
BrowserWindowFeatures::CreateBrowserWindowFeatures() {
  if (GetFactory()) {
    CHECK_IS_TEST();
    return GetFactory().Run();
  }
  // Constructor is protected.
  return base::WrapUnique(new BrowserWindowFeatures());
}

BrowserWindowFeatures::~BrowserWindowFeatures() = default;

// static
void BrowserWindowFeatures::ReplaceBrowserWindowFeaturesForTesting(
    BrowserWindowFeaturesFactory factory) {
  BrowserWindowFeatures::BrowserWindowFeaturesFactory& f = GetFactory();
  f = std::move(factory);
}

void BrowserWindowFeatures::Init(BrowserWindowInterface* browser) {
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
        product_specifications_entry_point_controller_ = std::make_unique<
            commerce::ProductSpecificationsEntryPointController>(browser);
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
      DCHECK(features::IsTabSearchMoving());
      glic_nudge_controller_ =
          std::make_unique<tabs::GlicNudgeController>(browser);
      glic_iph_controller_ = std::make_unique<glic::GlicIphController>(browser);
    }
#endif  // BUILDFLAG(ENABLE_GLIC)
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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  if (base::FeatureList::IsEnabled(features::kPdfInfoBar)) {
    pdf_infobar_controller_ = std::make_unique<PdfInfoBarController>(browser);
  }
#endif
}

void BrowserWindowFeatures::InitPostWindowConstruction(Browser* browser) {
  // Features that are only enabled for normal browser windows (e.g. a window
  // with an omnibox and a tab strip). By default most features should be
  // instantiated in this block.
  if (browser->is_type_normal()) {
    if (IsChromeLabsEnabled()) {
      chrome_labs_coordinator_ =
          std::make_unique<ChromeLabsCoordinator>(browser);
    }

    send_tab_to_self_toolbar_bubble_controller_ = std::make_unique<
        send_tab_to_self::SendTabToSelfToolbarBubbleController>(browser);

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
      if (BrowserView* browser_view =
              BrowserView::GetBrowserViewForBrowser(browser)) {
        location_bar = browser_view->GetLocationBarView();
      }
      lens_overlay_entry_point_controller_->Initialize(
          browser, browser->command_controller(), location_bar);
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
      if (BrowserView* browser_view =
              BrowserView::GetBrowserViewForBrowser(browser)) {
        tab_search_toolbar_button_controller_ =
            std::make_unique<TabSearchToolbarButtonController>(
                browser_view, browser_view->GetTabSearchBubbleHost());
      }
    }
  }

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

  bookmarks_side_panel_coordinator_ =
      std::make_unique<BookmarksSidePanelCoordinator>();

  side_panel_coordinator_->Init(browser_view->browser());

  extension_side_panel_manager_ =
      std::make_unique<extensions::ExtensionSidePanelManager>(
          browser_view->browser(),
          side_panel_coordinator_->GetWindowRegistry());

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
    }
#endif

    memory_saver_opt_in_iph_controller_ =
        std::make_unique<MemorySaverOptInIPHController>(
            browser_view->browser());

    if (media_router::MediaRouterEnabled(browser_view->browser()->profile())) {
      cast_browser_controller_ =
          std::make_unique<media_router::CastBrowserController>(
              browser_view->browser());
    }
  }

  if (download::IsDownloadBubbleEnabled()) {
    download_toolbar_ui_controller_ =
        std::make_unique<DownloadToolbarUIController>(browser_view);
  }

  if (browser_view->browser()->GetTabStripModel()->SupportsTabGroups() &&
      tab_groups::SavedTabGroupUtils::SupportsSharedTabGroups() &&
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(
          browser_view->GetProfile())) {
    shared_tab_group_feedback_controller_ =
        std::make_unique<tab_groups::SharedTabGroupFeedbackController>(
            browser_view);
  }

  if (base::FeatureList::IsEnabled(ntp_features::kNtpFooter)) {
    new_tab_footer_controller_ =
        std::make_unique<new_tab_footer::NewTabFooterController>(
            browser_view->browser(), browser_view->new_tab_footer_web_view());
  }
}

void BrowserWindowFeatures::TearDownPreBrowserViewDestruction() {
  memory_saver_opt_in_iph_controller_.reset();
  lens_overlay_entry_point_controller_.reset();
  tab_search_toolbar_button_controller_.reset();

#if BUILDFLAG(ENABLE_GLIC)
  glic_button_controller_.reset();
#endif

  if (download_toolbar_ui_controller_) {
    download_toolbar_ui_controller_->TearDownPreBrowserViewDestruction();
  }

  // TODO(crbug.com/346148093): This logic should not be gated behind a
  // conditional.
  if (side_panel_coordinator_) {
    side_panel_coordinator_->TearDownPreBrowserViewDestruction();
  }

  if (mv2_disabled_dialog_controller_) {
    mv2_disabled_dialog_controller_->TearDown();
  }

  if (shared_tab_group_feedback_controller_) {
    shared_tab_group_feedback_controller_->TearDown();
  }

  if (chrome_labs_coordinator_) {
    chrome_labs_coordinator_->TearDown();
  }

  if (new_tab_footer_controller_) {
    new_tab_footer_controller_->TearDown();
  }
}

SidePanelUI* BrowserWindowFeatures::side_panel_ui() {
  return side_panel_coordinator_.get();
}

ToastController* BrowserWindowFeatures::toast_controller() {
  return toast_service_ ? toast_service_->toast_controller() : nullptr;
}

BrowserWindowFeatures::BrowserWindowFeatures() = default;
