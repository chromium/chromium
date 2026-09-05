// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class is used to construct and hold window-scoped state.
//
// This class exists for 3 reasons:
//  (1) It provides explicit construction and destruction ordering.
//  (2) It allows for dependency-injection at construction time of features.
//  (3) It pairs with the UnownedUserData design pattern to ensure dependencies
//      are precisely specified by BUILD.gn files. This prevents circular
//      dependencies.
//
// If you want to make a new BrowserWindowFeature, following these steps:
//  (1) Make a regular C++ class. It should NOT inherit from SupportsUserData.
//  (2) Forward declare the class, and add a std::unique_ptr member to this
//      header file.
//  (3) Construct the member in browser_window_features.cc.
//  (4) If consumers need to access the feature, expose it via
//      BrowserWindowInterface and UnownedUserData.
//
// For more details on UnownedUserData, see ui/base/unowned_user_data/README.md.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_FEATURES_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_FEATURES_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/common/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/unowned_user_data/user_data_factory.h"

class ActorBorderViewController;
class ActorUiWindowController;
class BookmarkBarController;
class BookmarksSidePanelCoordinator;
class BookmarksServiceFeature;
class BreadcrumbManagerBrowserAgent;

class BrowserActions;
class BrowserActiveStateManager;
class BrowserAnimationController;
class BrowserContentSettingBubbleModelDelegate;
class BrowserElements;
class BrowserFocusController;
class BrowserInstantController;
class BrowserLiveTabContext;
namespace sessions {
class LiveTabContext;
}
class BrowserLocationBarModelDelegate;
class BrowserSelectFileDialogController;
class BrowserSyncedWindowDelegate;
class BrowserUiController;
class BrowserUserEducationInterface;
class BrowserView;
class BrowserWebContentsDelegate;
class BrowserWindowFullscreenController;
class BrowserWindowInterface;
class BrowserWindowModalDialogDelegate;
class BrowserWindowThemeObserver;
class BrowserWindowZoomObserver;
class CallToActionLock;
class ChromeLabsCoordinator;
class ColorProviderBrowserHelper;
class CommentsSidePanelCoordinator;
class ContentsBorderController;
class ContextHighlightWindowFeature;

class CookieControlsBubbleCoordinator;
class DataSharingBubbleController;
class DesktopBrowserWindowCapabilities;
class DevtoolsUIController;
class EmbedderBrowserWindowFeatures;
class ExclusiveAccessManager;
class ExtensionInstalledWatcher;
class ExtensionKeybindingRegistryViews;
class FindBarController;
class FindBarOwner;
class FullscreenControlHost;
class HistoryClustersSidePanelCoordinator;
class HistorySidePanelCoordinator;
class ImmersiveModeController;
class IncognitoClearBrowsingDataDialogCoordinator;
class InitialWebUIManager;
class InitialWebUIWindowMetricsManager;
class IOSPromoController;
class LocationBar;
class LocationBarModel;
class MemorySaverOptInIPHController;
class PinnedToolbarActions;
class ProfileMenuCoordinator;
class OrganizerPanelStateController;
class ReadingListSidePanelCoordinator;
class RecentActivityBubbleCoordinator;
class ScrimViewController;
class SearchboxContextData;
class SessionServiceBrowserHelper;
class SharingWindowController;
class SidePanelCoordinator;
class SidePanelRegistry;
class SigninViewController;
class SplitViewIphController;
class TabDragServiceFeature;
class TabListBridge;

namespace send_tab_to_self {
class SendTabToSelfIphController;
}  // namespace send_tab_to_self
class TabMenuModelDelegate;
class TabStripModel;
class TabStripServiceFeature;
class TabsFromOtherDevicesSidePanelCoordinator;
class ToastService;
class TranslateBubbleController;
class UIControllerFactory;
class UnloadController;
class UpgradeNotificationController;
class VerticalTabIphController;
class WebUIBrowserExclusiveAccessContext;
class WebUIBrowserSidePanelUI;
class WindowFeatureController;
class WindowMetadataController;
class ZoomBubbleCoordinator;
class ZoomBubbleManager;

#if BUILDFLAG(IS_WIN)
class WindowsTaskbarIconUpdater;
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
class ProfileCustomizationBubbleSyncController;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if !BUILDFLAG(IS_CHROMEOS)
class DownloadToolbarUIController;
#endif

#if defined(USE_AURA)
class OverscrollPrefManager;
#endif  // defined(USE_AURA)

#if BUILDFLAG(ENABLE_EXTENSIONS) && (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC))
class DefaultSearchExtensionControlledController;
#endif

namespace actions {
class ActionItem;
}  // namespace actions

#if BUILDFLAG(IS_CHROMEOS)
namespace ash::boca {
class OnTaskLockedController;
}  // namespace ash::boca

namespace chromeos {
class LockedStateController;
}  // namespace chromeos
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace chrome {
class BrowserCommandController;
}  // namespace chrome

namespace content_settings {
class CookieControlsController;
}  // namespace content_settings

namespace contextual_tasks {
class ContextualTasksBrowserController;
}  // namespace contextual_tasks

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
namespace default_browser {
class PinInfoBarController;
}  // namespace default_browser
#endif

namespace enterprise_data_protection {
class DataProtectionUIController;
}  // namespace enterprise_data_protection

namespace extensions {
class BrowserExtensionWindowController;
#if BUILDFLAG(ENABLE_EXTENSIONS)
class ExtensionBrowserWindowHelper;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
class ExtensionSidePanelManager;
}  // namespace extensions

namespace glic {
class GlicIphController;
class GlicSplitButtonController;
}  // namespace glic

namespace lens {
class LensOverlayEntryPointController;
class LensRegionSearchController;
}  // namespace lens

namespace media_router {
class CastBrowserController;
}  // namespace media_router

namespace memory_saver {
class MemorySaverBubbleController;
}  // namespace memory_saver

namespace new_tab_footer {
class NewTabFooterController;
}  // namespace new_tab_footer

namespace omnibox {
class AiModePageActionController;
class OmniboxPopupCloser;
}  // namespace omnibox

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
namespace pdf::infobar {
class PdfInfoBarController;
}  // namespace pdf::infobar
#endif

namespace qrcode_generator {
class QRCodeWindowController;
}  // namespace qrcode_generator

namespace send_tab_to_self {
class SendTabToSelfToolbarBubbleController;
}  // namespace send_tab_to_self

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
namespace session_restore_infobar {
class SessionRestoreInfobarController;
}  // namespace session_restore_infobar
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

namespace sharing_hub {
class SharingHubWindowController;
}  // namespace sharing_hub

namespace skills {
class SkillsUiWindowController;
}  // namespace skills

namespace split_tabs {
class SplitTabHighlightController;
}  // namespace split_tabs

namespace tab_groups {
class DeletionDialogController;
class MostRecentSharedTabUpdateStore;
class SessionServiceTabGroupSyncObserver;
class SharedTabGroupFeedbackController;
}  // namespace tab_groups

namespace tabs {
class VerticalTabStripStateController;
}  // namespace tabs

namespace tabs_api {
class TabStripUIControllerImpl;
}  // namespace tabs_api

namespace ttc {
class AiOverlayDialogController;
}  // namespace ttc

namespace ui {
class AcceleratorProvider;
}  // namespace ui

namespace web_app {
class AppBrowserController;
}  // namespace web_app

// This class owns the core controllers for features that are scoped to a given
// browser window on desktop.
//
// To inject alternative versions of features or mocks for testing, make your
// feature compatible with `UnownedUserDataHost` and then use
// `GetUserDataFactoryForTesting()` to inject your test-specific feature
// object(s).
//
// Do not add more public accessors. Instead use the UnownedUserData design
// pattern, see ui/base/unowned_user_data/README.md.
// TODO(crbug.com/481268779a): Remove existing public accessors.
class BrowserWindowFeatures {
 public:
  BrowserWindowFeatures();
  ~BrowserWindowFeatures();

  BrowserWindowFeatures(const BrowserWindowFeatures&) = delete;
  BrowserWindowFeatures& operator=(const BrowserWindowFeatures&) = delete;

  // Called exactly once to initialize features. This is called prior to
  // instantiating BrowserView, to allow the view hierarchy to depend on state
  // in this class.
  void Init(BrowserWindowInterface* browser);

  // Called exactly once to initialize features that depend on the window object
  // being created.
  void InitPostWindowConstruction(BrowserWindowInterface* browser);

  // Called exactly once to initialize features that depend on the view
  // hierarchy in BrowserView.
  void InitPostBrowserViewConstruction(BrowserView* browser_view);

  // Called exactly once to tear down state that depends on the window object.
  void TearDownPreBrowserWindowDestruction();

  ui::AcceleratorProvider* accelerator_provider() {
    return accelerator_provider_;
  }

  chrome::BrowserCommandController* browser_command_controller() const {
    return browser_command_controller_.get();
  }

  ExtensionKeybindingRegistryViews* extension_keybinding_registry() {
    return extension_keybinding_registry_.get();
  }

  // Get the FindBarController for this browser window, creating it if it does
  // not yet exist.
  FindBarController* GetFindBarController();

  actions::ActionItem* GetRootActionItem();

  // Returns true if a FindBarController exists for this browser window.
  bool HasFindBarController() const;

  // Returns the LocationBar for this browser window. Currently delegates to
  // BrowserWindow::GetLocationBar() via downcast, but should eventually become
  // an owned member of BrowserWindowFeatures.
  LocationBar* location_bar();
  const LocationBar* location_bar() const;

  LocationBarModel* location_bar_model() { return location_bar_model_.get(); }
  const LocationBarModel* location_bar_model() const {
    return location_bar_model_.get();
  }
#if defined(UNIT_TEST)
  void swap_location_bar_models(
      std::unique_ptr<LocationBarModel>* location_bar_model) {
    location_bar_model->swap(location_bar_model_);
  }
#endif

  PinnedToolbarActions* pinned_toolbar_actions() {
    return pinned_toolbar_actions_;
  }

  TabStripModel* tab_strip_model() { return tab_strip_model_; }

  static ui::UserDataFactoryWithOwner<BrowserWindowInterface>&
  GetUserDataFactoryForTesting();

 private:
  static ui::UserDataFactoryWithOwner<BrowserWindowInterface>&
  GetUserDataFactory();

  // Members owned by all browser window types.
  std::unique_ptr<UnloadController> unload_controller_;
  std::unique_ptr<ActorBorderViewController> actor_border_view_controller_;
  std::unique_ptr<ttc::AiOverlayDialogController> ai_overlay_dialog_controller_;

  // Helper which handles bookmark app specific browser configuration.
  // This must be initialized before |command_controller_| to ensure the correct
  // set of commands are enabled.
  std::unique_ptr<web_app::AppBrowserController> app_browser_controller_;

  std::unique_ptr<BookmarkBarController> bookmark_bar_controller_;
  std::unique_ptr<BookmarksServiceFeature> bookmarks_service_feature_;
  std::unique_ptr<BookmarksSidePanelCoordinator>
      bookmarks_side_panel_coordinator_;

  // Listens for browser-related breadcrumb events to be added to crash reports.
  std::unique_ptr<BreadcrumbManagerBrowserAgent>
      breadcrumb_manager_browser_agent_;

  std::unique_ptr<BrowserActions> browser_actions_;
  std::unique_ptr<BrowserActiveStateManager> browser_active_state_manager_;
  std::unique_ptr<BrowserAnimationController> browser_animation_controller_;
  std::unique_ptr<chrome::BrowserCommandController> browser_command_controller_;
  std::unique_ptr<BrowserElements> browser_elements_;
  std::unique_ptr<BrowserFocusController> browser_focus_controller_;
  std::unique_ptr<BrowserSelectFileDialogController>
      browser_select_file_dialog_controller_;
  std::unique_ptr<BrowserUiController> browser_ui_controller_;
  std::unique_ptr<BrowserWebContentsDelegate> browser_web_contents_delegate_;
  std::unique_ptr<BrowserWindowModalDialogDelegate>
      browser_window_modal_dialog_delegate_;
  std::unique_ptr<BrowserWindowThemeObserver> browser_window_theme_observer_;
  std::unique_ptr<BrowserWindowZoomObserver> browser_window_zoom_observer_;
  std::unique_ptr<CallToActionLock> call_to_action_lock_;
  std::unique_ptr<ChromeLabsCoordinator> chrome_labs_coordinator_;
  std::unique_ptr<CommentsSidePanelCoordinator>
      comments_side_panel_coordinator_;

  // Helper which implements the ContentSettingBubbleModel interface.
  std::unique_ptr<BrowserContentSettingBubbleModelDelegate>
      content_setting_bubble_model_delegate_;

  std::unique_ptr<ContextHighlightWindowFeature>
      context_highlight_window_feature_;

  // Member order dependencies:
  //   glic_nudge_controller_ depends on tab_list_bridge_.
  //   extension_window_controller_ depends on tab_list_bridge_.
  std::unique_ptr<TabListBridge> tab_list_bridge_;

  std::unique_ptr<contextual_tasks::ContextualTasksBrowserController>
      contextual_tasks_browser_controller_;
  std::unique_ptr<CookieControlsBubbleCoordinator>
      cookie_controls_bubble_coordinator_;
  std::unique_ptr<content_settings::CookieControlsController>
      cookie_controls_controller_;
  std::unique_ptr<DataSharingBubbleController> data_sharing_bubble_controller_;

  // A collection of features specific to desktop versions of Chrome.
  // Member order dependencies:
  //   DesktopBrowserWindowCapabilities depends on modal dialog delegate.
  //   DesktopBrowserWindowCapabilities depends on unload_controller_.
  std::unique_ptr<DesktopBrowserWindowCapabilities>
      desktop_browser_window_capabilities_;

  std::unique_ptr<ExclusiveAccessManager> exclusive_access_manager_;
  std::unique_ptr<ExtensionInstalledWatcher> extension_installed_watcher_;

  // The class that registers for keyboard shortcuts for extension commands,
  // and its delegate.
  std::unique_ptr<ExtensionKeybindingRegistryViews>
      extension_keybinding_registry_;

  // Member order dependency:
  //   extension_window_controller_ depends on tab_list_bridge_.
  std::unique_ptr<extensions::BrowserExtensionWindowController>
      extension_window_controller_;

  // The Find Bar. This may be NULL if there is no Find Bar, and if it is
  // non-NULL, it may or may not be visible.
  std::unique_ptr<FindBarController> find_bar_controller_;

  std::unique_ptr<FindBarOwner> find_bar_owner_;
  std::unique_ptr<BrowserWindowFullscreenController> fullscreen_controller_;
  std::unique_ptr<glic::GlicIphController> glic_iph_controller_;
  std::unique_ptr<glic::GlicSplitButtonController>
      glic_split_button_controller_;
  std::unique_ptr<HistoryClustersSidePanelCoordinator>
      history_clusters_side_panel_coordinator_;
  std::unique_ptr<HistorySidePanelCoordinator> history_side_panel_coordinator_;
  std::unique_ptr<IncognitoClearBrowsingDataDialogCoordinator>
      incognito_clear_browsing_data_dialog_coordinator_;
  std::unique_ptr<InitialWebUIManager> initial_web_ui_manager_;
  std::unique_ptr<InitialWebUIWindowMetricsManager>
      initial_webui_window_metrics_manager_;
  std::unique_ptr<BrowserInstantController> instant_controller_;
  std::unique_ptr<IOSPromoController> ios_promo_controller_;
  std::unique_ptr<lens::LensOverlayEntryPointController>
      lens_overlay_entry_point_controller_;
  std::unique_ptr<lens::LensRegionSearchController>
      lens_region_search_controller_;

  // Helper which implements the LiveTabContext interface.
  std::unique_ptr<BrowserLiveTabContext> live_tab_context_;

  // Helper which implements the LocationBarModelDelegate interface.
  std::unique_ptr<BrowserLocationBarModelDelegate> location_bar_model_delegate_;

  // The model for the toolbar view.
  std::unique_ptr<LocationBarModel> location_bar_model_;

  std::unique_ptr<memory_saver::MemorySaverBubbleController>
      memory_saver_bubble_controller_;
  std::unique_ptr<tab_groups::MostRecentSharedTabUpdateStore>
      most_recent_shared_tab_update_store_;
  std::unique_ptr<ProfileMenuCoordinator> profile_menu_coordinator_;
  std::unique_ptr<OrganizerPanelStateController>
      organizer_panel_state_controller_;
  std::unique_ptr<qrcode_generator::QRCodeWindowController>
      qrcode_window_controller_;
  std::unique_ptr<ReadingListSidePanelCoordinator>
      reading_list_side_panel_coordinator_;
  std::unique_ptr<RecentActivityBubbleCoordinator>
      recent_activity_bubble_coordinator_;
  std::unique_ptr<SearchboxContextData> searchbox_context_data_;
  std::unique_ptr<send_tab_to_self::SendTabToSelfToolbarBubbleController>
      send_tab_to_self_toolbar_bubble_controller_;
  std::unique_ptr<SessionServiceBrowserHelper> session_service_browser_helper_;
  std::unique_ptr<tab_groups::SessionServiceTabGroupSyncObserver>
      session_service_tab_group_sync_observer_;
  std::unique_ptr<sharing_hub::SharingHubWindowController>
      sharing_hub_window_controller_;
  std::unique_ptr<SharingWindowController> sharing_window_controller_;
  std::unique_ptr<SidePanelRegistry> side_panel_registry_;
  std::unique_ptr<SigninViewController> signin_view_controller_;
  std::unique_ptr<SplitViewIphController> split_view_iph_controller_;
  std::unique_ptr<BrowserSyncedWindowDelegate> synced_window_delegate_;
  std::unique_ptr<TabDragServiceFeature> tab_drag_service_feature_;
  std::unique_ptr<tab_groups::DeletionDialogController>
      tab_group_deletion_dialog_controller_;
  std::unique_ptr<TabMenuModelDelegate> tab_menu_model_delegate_;

  // This is an experimental API that interacts with the TabStripModel.
  std::unique_ptr<TabStripServiceFeature> tab_strip_service_feature_;

  // Controller for managing TabStrip UI decoupled TabStrip platform.
  std::unique_ptr<tabs_api::TabStripUIControllerImpl> tab_strip_ui_controller_;

  std::unique_ptr<TabsFromOtherDevicesSidePanelCoordinator>
      tabs_from_other_devices_side_panel_coordinator_;
  std::unique_ptr<ToastService> toast_service_;
  std::unique_ptr<TranslateBubbleController> translate_bubble_controller_;
  std::unique_ptr<UpgradeNotificationController>
      upgrade_notification_controller_;
  std::unique_ptr<BrowserUserEducationInterface> user_education_;
  std::unique_ptr<send_tab_to_self::SendTabToSelfIphController>
      send_tab_to_self_iph_controller_;
  std::unique_ptr<VerticalTabIphController> vertical_tab_iph_controller_;
  std::unique_ptr<tabs::VerticalTabStripStateController>
      vertical_tab_strip_state_controller_;

  // Must come after fullscreen_controller_.
  std::unique_ptr<WindowFeatureController> window_feature_controller_;
  std::unique_ptr<ImmersiveModeController> immersive_mode_controller_;

  std::unique_ptr<WindowMetadataController> window_metadata_controller_;

  // Must come before zoom_bubble_coordinator_.
  std::unique_ptr<ZoomBubbleManager> zoom_bubble_manager_;
  std::unique_ptr<ZoomBubbleCoordinator> zoom_bubble_coordinator_;

  // Members owned only when a BrowserView is attached.
  std::unique_ptr<ActorUiWindowController> actor_ui_window_controller_;
  std::unique_ptr<omnibox::AiModePageActionController>
      ai_mode_page_action_controller_;
  std::unique_ptr<media_router::CastBrowserController> cast_browser_controller_;
  std::unique_ptr<ColorProviderBrowserHelper> color_provider_browser_helper_;
  std::unique_ptr<ContentsBorderController> contents_border_controller_;
  std::unique_ptr<enterprise_data_protection::DataProtectionUIController>
      data_protection_ui_controller_;
  std::unique_ptr<DevtoolsUIController> devtools_ui_controller_;

  // The window-scoped extension side-panel manager. There is a separate
  // tab-scoped extension side-panel manager.
  std::unique_ptr<extensions::ExtensionSidePanelManager>
      extension_side_panel_manager_;

  std::unique_ptr<FullscreenControlHost> fullscreen_control_host_;
  std::unique_ptr<MemorySaverOptInIPHController>
      memory_saver_opt_in_iph_controller_;
  std::unique_ptr<new_tab_footer::NewTabFooterController>
      new_tab_footer_controller_;
  std::unique_ptr<omnibox::OmniboxPopupCloser> omnibox_popup_closer_;
  std::unique_ptr<ScrimViewController> scrim_view_controller_;
  std::unique_ptr<tab_groups::SharedTabGroupFeedbackController>
      shared_tab_group_feedback_controller_;
  std::unique_ptr<SidePanelCoordinator> side_panel_coordinator_;
  std::unique_ptr<UIControllerFactory> ui_controller_factory_;
  std::unique_ptr<skills::SkillsUiWindowController>
      skills_ui_window_controller_;
  std::unique_ptr<split_tabs::SplitTabHighlightController>
      split_tab_highlight_controller_;

  // Members owned only when a WebUIBrowserWindow is used.
  std::unique_ptr<WebUIBrowserExclusiveAccessContext>
      webui_browser_exclusive_access_context_;
  std::unique_ptr<WebUIBrowserSidePanelUI> webui_browser_side_panel_ui_;

  // Platform-specific members.
#if BUILDFLAG(IS_WIN)
  std::unique_ptr<WindowsTaskbarIconUpdater> windows_taskbar_icon_updater_;
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  std::unique_ptr<pdf::infobar::PdfInfoBarController> pdf_infobar_controller_;
  std::unique_ptr<default_browser::PinInfoBarController>
      pin_infobar_controller_;
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  std::unique_ptr<ProfileCustomizationBubbleSyncController>
      profile_customization_bubble_sync_controller_;
  std::unique_ptr<session_restore_infobar::SessionRestoreInfobarController>
      session_restore_infobar_controller_;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<chromeos::LockedStateController> locked_state_controller_;
  std::unique_ptr<ash::boca::OnTaskLockedController> on_task_locked_controller_;
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<DownloadToolbarUIController> download_toolbar_ui_controller_;
#endif

#if defined(USE_AURA)
  std::unique_ptr<OverscrollPrefManager> overscroll_pref_manager_;
#endif  // defined(USE_AURA)

#if BUILDFLAG(ENABLE_EXTENSIONS)
  std::unique_ptr<extensions::ExtensionBrowserWindowHelper>
      extension_browser_window_helper_;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  std::unique_ptr<DefaultSearchExtensionControlledController>
      default_search_extension_controlled_controller_;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  // Non-owning references.
  // TODO(webium): Current both BrowserView and WebUIBrowserWindow implement
  // AcceleratorProvider. Consider eliminating this inheritance and composing
  // this functionality into its own class.
  raw_ptr<ui::AcceleratorProvider> accelerator_provider_;

  // TODO(crbug.com/423956131): Remove this.
  raw_ptr<BrowserWindowInterface> browser_ = nullptr;

  raw_ptr<PinnedToolbarActions> pinned_toolbar_actions_ = nullptr;
  raw_ptr<TabStripModel> tab_strip_model_;

  // Embedder features. Must be declared last.
  // Keep this member last to ensure embedder features are torn down first, in
  // reverse order of initialization.
  std::unique_ptr<EmbedderBrowserWindowFeatures>
      embedder_browser_window_features_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_FEATURES_H_
