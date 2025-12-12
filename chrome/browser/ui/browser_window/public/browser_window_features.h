// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_FEATURES_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_FEATURES_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/common/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/unowned_user_data/user_data_factory.h"

#if BUILDFLAG(ENABLE_GLIC)
namespace glic {
class GlicButtonController;
class GlicIphController;
class GlicLegacySidePanelCoordinator;
}  // namespace glic

namespace tabs {
class GlicActorTaskIconController;
class GlicActorNudgeController;
}  // namespace tabs
#endif

class ActorUiWindowController;

class ActorBorderViewController;
class ActorTaskListBubbleController;
class BookmarkBarController;
class BookmarksSidePanelCoordinator;
class BreadcrumbManagerBrowserAgent;
class Browser;
class BrowserActions;
class BrowserContentSettingBubbleModelDelegate;
class BrowserElements;
class BrowserInstantController;
class BrowserLiveTabContext;
class BrowserLocationBarModelDelegate;
class BrowserSyncedWindowDelegate;
class BrowserUserEducationInterface;
class BrowserView;
class BrowserWindowInterface;
class ChromeLabsCoordinator;
class ColorProviderBrowserHelper;
class LocationBar;
class CommentsSidePanelCoordinator;
class ContentsBorderController;
class ContextualTasksEphemeralButtonController;
class CookieControlsBubbleCoordinator;
class DataSharingBubbleController;
class DesktopBrowserWindowCapabilities;
class DevtoolsUIController;
class EmbedderBrowserWindowFeatures;
class ExtensionKeybindingRegistryViews;
class ExclusiveAccessManager;
class FindBarController;
class FindBarOwner;
class FullscreenControlHost;
class HistoryClustersSidePanelCoordinator;
class HistorySidePanelCoordinator;
class IncognitoClearBrowsingDataDialogCoordinator;
class ImmersiveModeController;
class IOSPromoController;
class LocationBarModel;
class MemorySaverOptInIPHController;
class PinnedToolbarActionsController;
class ProfileMenuCoordinator;
class ReadingListSidePanelCoordinator;
class RecentActivityBubbleCoordinator;
class BrowserSelectFileDialogController;
class ScrimViewController;
class SearchboxContextData;
class SidePanelCoordinator;
class SidePanelRegistry;
class SidePanelUI;
class SigninViewController;
class SplitViewIphController;
class TabMenuModelDelegate;
class TabSearchToolbarButtonController;
class TabListBridge;
class TabStripModel;
class TabStripServiceFeature;
class ToastController;
class ToastService;
class TranslateBubbleController;
class UpgradeNotificationController;
class WebUIBrowserExclusiveAccessContext;
class WebUIBrowserSidePanelUI;
class ZoomBubbleCoordinator;

#if BUILDFLAG(IS_WIN)
class WindowsTaskbarIconUpdater;
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
namespace pdf::infobar {
class PdfInfoBarController;
}  // namespace pdf::infobar
namespace default_browser {
class PinInfoBarController;
}  // namespace default_browser
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
class ProfileCustomizationBubbleSyncController;
namespace session_restore_infobar {
class SessionRestoreInfobarController;
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if !BUILDFLAG(IS_CHROMEOS)
class DownloadToolbarUIController;
#endif

#if defined(USE_AURA)
class OverscrollPrefManager;
#endif  // defined(USE_AURA)

namespace extensions {
class BrowserExtensionWindowController;
#if BUILDFLAG(ENABLE_EXTENSIONS)
class ExtensionBrowserWindowHelper;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
class ExtensionSidePanelManager;
class Mv2DisabledDialogController;
}  // namespace extensions

namespace tabs {
class TabDeclutterController;
class VerticalTabStripStateController;
}  // namespace tabs

namespace chrome {
class BrowserCommandController;
}  // namespace chrome

namespace commerce {
class ProductSpecificationsEntryPointController;
}  // namespace commerce

namespace contextual_tasks {
class ActiveTaskContextProvider;
class ContextualTasksSidePanelCoordinator;
}  // namespace contextual_tasks

namespace tabs {
class GlicNudgeController;
}  // namespace tabs

namespace enterprise_data_protection {
class DataProtectionUIController;
}  // namespace enterprise_data_protection

namespace tab_groups {
class DeletionDialogController;
}  // namespace tab_groups

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

namespace tab_groups {
class SessionServiceTabGroupSyncObserver;
class SharedTabGroupFeedbackController;
class MostRecentSharedTabUpdateStore;
}  // namespace tab_groups

namespace send_tab_to_self {
class SendTabToSelfToolbarBubbleController;
}  // namespace send_tab_to_self

namespace split_tabs {
class SplitTabHighlightController;
}  // namespace split_tabs

namespace ui {
class AcceleratorProvider;
}  // namespace ui

namespace web_app {
class AppBrowserController;
}  // namespace web_app

namespace omnibox {
class AiModePageActionController;
class OmniboxPopupCloser;
}  // namespace omnibox

// This class owns the core controllers for features that are scoped to a given
// browser window on desktop.
//
// To inject alternative versions of features or mocks for testing, make your
// feature compatible with `UnownedUserDataHost` and then use
// `GetUserDataFactoryForTesting()` to inject your test-specific feature
// object(s).
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
  void InitPostWindowConstruction(Browser* browser);

  // Called exactly once to initialize features that depend on the view
  // hierarchy in BrowserView.
  void InitPostBrowserViewConstruction(BrowserView* browser_view);

  // Called exactly once to tear down state that depends on the window object.
  void TearDownPreBrowserWindowDestruction();

  BrowserActions* browser_actions() { return browser_actions_.get(); }

  chrome::BrowserCommandController* browser_command_controller() {
    return browser_command_controller_.get();
  }

  extensions::Mv2DisabledDialogController*
  mv2_disabled_dialog_controller_for_testing() {
    return mv2_disabled_dialog_controller_.get();
  }

  ChromeLabsCoordinator* chrome_labs_coordinator() {
    return chrome_labs_coordinator_.get();
  }

  contextual_tasks::ActiveTaskContextProvider*
  contextual_tasks_active_task_context_provider() {
    return contextual_tasks_active_task_context_provider_.get();
  }

  media_router::CastBrowserController* cast_browser_controller() {
    return cast_browser_controller_.get();
  }

  HistorySidePanelCoordinator* history_side_panel_coordinator() {
    return history_side_panel_coordinator_.get();
  }

  BookmarksSidePanelCoordinator* bookmarks_side_panel_coordinator() {
    return bookmarks_side_panel_coordinator_.get();
  }

  CommentsSidePanelCoordinator* comments_side_panel_coordinator() {
    return comments_side_panel_coordinator_.get();
  }

#if BUILDFLAG(ENABLE_GLIC)
  glic::GlicLegacySidePanelCoordinator* glic_side_panel_coordinator() {
    return glic_side_panel_coordinator_.get();
  }

  glic::GlicIphController* glic_iph_controller() {
    return glic_iph_controller_.get();
  }
#endif

  PinnedToolbarActionsController* pinned_toolbar_actions_controller() {
    return pinned_toolbar_actions_controller_.get();
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  pdf::infobar::PdfInfoBarController* pdf_infobar_controller() {
    return pdf_infobar_controller_.get();
  }
  default_browser::PinInfoBarController* pin_infobar_controller() {
    return pin_infobar_controller_.get();
  }
#endif

  // TODO(crbug.com/346158959): For historical reasons, side_panel_ui is an
  // abstract base class that contains some, but not all of the public interface
  // of SidePanelCoordinator. One of the accessors side_panel_ui() or
  // side_panel_coordinator() should be removed. For consistency with the rest
  // of this class, we use lowercase_with_underscores even though the
  // implementation is not inlined.
  SidePanelUI* side_panel_ui();

  SidePanelCoordinator* side_panel_coordinator() {
    return side_panel_coordinator_.get();
  }

  lens::LensOverlayEntryPointController* lens_overlay_entry_point_controller() {
    return lens_overlay_entry_point_controller_.get();
  }

  lens::LensRegionSearchController* lens_region_search_controller() {
    return lens_region_search_controller_.get();
  }

  tabs::TabDeclutterController* tab_declutter_controller() {
    return tab_declutter_controller_.get();
  }

  tabs::VerticalTabStripStateController* vertical_tab_strip_state_controller() {
    return vertical_tab_strip_state_controller_.get();
  }

  tabs::GlicNudgeController* glic_nudge_controller() {
    return glic_nudge_controller_.get();
  }

  TabStripModel* tab_strip_model() { return tab_strip_model_; }

  // Returns a pointer to the ToastController for the browser window. This can
  // return nullptr for non-normal browser windows because toasts are not
  // supported for those cases.
  ToastController* toast_controller();

  // Returns a pointer to the ToastService for the browser window. This can
  // return nullptr for non-normal browser windows because toasts are not
  // supported for those cases.
  ToastService* toast_service() { return toast_service_.get(); }

  send_tab_to_self::SendTabToSelfToolbarBubbleController*
  send_tab_to_self_toolbar_bubble_controller() {
    return send_tab_to_self_toolbar_bubble_controller_.get();
  }

  extensions::ExtensionSidePanelManager* extension_side_panel_manager() {
    return extension_side_panel_manager_.get();
  }

  ExtensionKeybindingRegistryViews* extension_keybinding_registry() {
    return extension_keybinding_registry_.get();
  }

#if !BUILDFLAG(IS_CHROMEOS)
  DownloadToolbarUIController* download_toolbar_ui_controller() {
    return download_toolbar_ui_controller_.get();
  }
#endif

  tab_groups::MostRecentSharedTabUpdateStore*
  most_recent_shared_tab_update_store() {
    return most_recent_shared_tab_update_store_.get();
  }

  memory_saver::MemorySaverBubbleController* memory_saver_bubble_controller() {
    return memory_saver_bubble_controller_.get();
  }

  tab_groups::SharedTabGroupFeedbackController*
  shared_tab_group_feedback_controller() {
    return shared_tab_group_feedback_controller_.get();
  }

  TabSearchToolbarButtonController* tab_search_toolbar_button_controller() {
    return tab_search_toolbar_button_controller_.get();
  }

  BrowserSyncedWindowDelegate* synced_window_delegate() {
    return synced_window_delegate_.get();
  }

  TabMenuModelDelegate* tab_menu_model_delegate() {
    return tab_menu_model_delegate_.get();
  }

  tab_groups::DeletionDialogController* tab_group_deletion_dialog_controller() {
    return tab_group_deletion_dialog_controller_.get();
  }

  SigninViewController* signin_view_controller() {
    return signin_view_controller_.get();
  }

  // Only fetch the tab_strip_service to register a pending receiver.
  TabStripServiceFeature* tab_strip_service_feature() {
    return tab_strip_service_feature_.get();
  }

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

  // Returns the LocationBar for this browser window. Currently delegates to
  // BrowserWindow::GetLocationBar() via downcast, but should eventually become
  // an owned member of BrowserWindowFeatures.
  LocationBar* location_bar();
  const LocationBar* location_bar() const;

  ReadingListSidePanelCoordinator* reading_list_side_panel_coordinator() {
    return reading_list_side_panel_coordinator_.get();
  }

  new_tab_footer::NewTabFooterController* new_tab_footer_controller() {
    return new_tab_footer_controller_.get();
  }

  DevtoolsUIController* devtools_ui_controller() {
    return devtools_ui_controller_.get();
  }

  split_tabs::SplitTabHighlightController* split_tab_highlight_controller() {
    return split_tab_highlight_controller_.get();
  }

  ContentsBorderController* contents_border_controller() {
    return contents_border_controller_.get();
  }

  ProfileMenuCoordinator* profile_menu_coordinator() {
    return profile_menu_coordinator_.get();
  }

  IncognitoClearBrowsingDataDialogCoordinator*
  incognito_clear_browsing_data_dialog_coordinator() {
    return incognito_clear_browsing_data_dialog_coordinator_.get();
  }

#if defined(USE_AURA)
  OverscrollPrefManager* overscroll_pref_manager() {
    return overscroll_pref_manager_.get();
  }
#endif  // defined(USE_AURA)

  BrowserSelectFileDialogController* browser_select_file_dialog_controller() {
    return browser_select_file_dialog_controller_.get();
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  ProfileCustomizationBubbleSyncController*
  profile_customization_bubble_sync_controller() {
    return profile_customization_bubble_sync_controller_.get();
  }

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  // Get the FindBarController for this browser window, creating it if it does
  // not yet exist.
  FindBarController* GetFindBarController();

  // Returns true if a FindBarController exists for this browser window.
  bool HasFindBarController() const;

  WebUIBrowserExclusiveAccessContext* webui_browser_exclusive_access_context() {
    return webui_browser_exclusive_access_context_.get();
  }

  ExclusiveAccessManager* exclusive_access_manager() {
    return exclusive_access_manager_.get();
  }

  FullscreenControlHost* fullscreen_control_host() {
    return fullscreen_control_host_.get();
  }

  HistoryClustersSidePanelCoordinator*
  history_clusters_side_panel_coordinator() {
    return history_clusters_side_panel_coordinator_.get();
  }

  UpgradeNotificationController* upgrade_notification_controller() {
    return upgrade_notification_controller_.get();
  }

  BrowserContentSettingBubbleModelDelegate*
  content_setting_bubble_model_delegate() {
    return content_setting_bubble_model_delegate_.get();
  }

  BrowserLiveTabContext* live_tab_context() { return live_tab_context_.get(); }

  ui::AcceleratorProvider* accelerator_provider() {
    return accelerator_provider_;
  }

  FindBarOwner* find_bar_owner() { return find_bar_owner_.get(); }

  SearchboxContextData* searchbox_context_data() {
    return searchbox_context_data_.get();
  }

  omnibox::OmniboxPopupCloser* omnibox_popup_closer() {
    return omnibox_popup_closer_.get();
  }

  static ui::UserDataFactoryWithOwner<BrowserWindowInterface>&
  GetUserDataFactoryForTesting();

 private:
  class ExtensionKeybindingRegistryDelegateTabStrip;

  static ui::UserDataFactoryWithOwner<BrowserWindowInterface>&
  GetUserDataFactory();

  // A collection of features specific to desktop versions of Chrome.
  std::unique_ptr<DesktopBrowserWindowCapabilities>
      desktop_browser_window_capabilities_;

  // Features that are per-browser window will each have a controller. e.g.
  // std::unique_ptr<FooFeature> foo_feature_;

  // Helper which handles bookmark app specific browser configuration.
  // This must be initialized before |command_controller_| to ensure the correct
  // set of commands are enabled.
  std::unique_ptr<web_app::AppBrowserController> app_browser_controller_;

  std::unique_ptr<BrowserActions> browser_actions_;

  std::unique_ptr<chrome::BrowserCommandController> browser_command_controller_;

  std::unique_ptr<BrowserElements> browser_elements_;

  std::unique_ptr<BookmarkBarController> bookmark_bar_controller_;

  std::unique_ptr<BrowserInstantController> instant_controller_;

  std::unique_ptr<send_tab_to_self::SendTabToSelfToolbarBubbleController>
      send_tab_to_self_toolbar_bubble_controller_;

  std::unique_ptr<ChromeLabsCoordinator> chrome_labs_coordinator_;

  std::unique_ptr<commerce::ProductSpecificationsEntryPointController>
      product_specifications_entry_point_controller_;

  std::unique_ptr<ImmersiveModeController> immersive_mode_controller_;

  std::unique_ptr<WebUIBrowserExclusiveAccessContext>
      webui_browser_exclusive_access_context_;

  std::unique_ptr<ExclusiveAccessManager> exclusive_access_manager_;

  std::unique_ptr<FullscreenControlHost> fullscreen_control_host_;

  std::unique_ptr<IOSPromoController> ios_promo_controller_;

  std::unique_ptr<lens::LensOverlayEntryPointController>
      lens_overlay_entry_point_controller_;

  std::unique_ptr<lens::LensRegionSearchController>
      lens_region_search_controller_;

  std::unique_ptr<extensions::Mv2DisabledDialogController>
      mv2_disabled_dialog_controller_;

  std::unique_ptr<tabs::TabDeclutterController> tab_declutter_controller_;

  std::unique_ptr<tabs::VerticalTabStripStateController>
      vertical_tab_strip_state_controller_;

  std::unique_ptr<MemorySaverOptInIPHController>
      memory_saver_opt_in_iph_controller_;

  std::unique_ptr<HistorySidePanelCoordinator> history_side_panel_coordinator_;

  std::unique_ptr<BookmarksSidePanelCoordinator>
      bookmarks_side_panel_coordinator_;

  std::unique_ptr<CommentsSidePanelCoordinator>
      comments_side_panel_coordinator_;

  std::unique_ptr<PinnedToolbarActionsController>
      pinned_toolbar_actions_controller_;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  std::unique_ptr<pdf::infobar::PdfInfoBarController> pdf_infobar_controller_;

  std::unique_ptr<default_browser::PinInfoBarController>
      pin_infobar_controller_;
#endif

  std::unique_ptr<ScrimViewController> scrim_view_controller_;

  std::unique_ptr<SidePanelRegistry> side_panel_registry_;

  std::unique_ptr<SidePanelCoordinator> side_panel_coordinator_;

  std::unique_ptr<WebUIBrowserSidePanelUI> webui_browser_side_panel_ui_;

  std::unique_ptr<tab_groups::SessionServiceTabGroupSyncObserver>
      session_service_tab_group_sync_observer_;

  raw_ptr<TabStripModel> tab_strip_model_;
  std::unique_ptr<ToastService> toast_service_;

  // The window-scoped extension side-panel manager. There is a separate
  // tab-scoped extension side-panel manager.
  std::unique_ptr<extensions::ExtensionSidePanelManager>
      extension_side_panel_manager_;

  // The class that registers for keyboard shortcuts for extension commands,
  // and its delegate.
  std::unique_ptr<ExtensionKeybindingRegistryDelegateTabStrip>
      extension_keybinding_delegate_;
  std::unique_ptr<ExtensionKeybindingRegistryViews>
      extension_keybinding_registry_;

  std::unique_ptr<media_router::CastBrowserController> cast_browser_controller_;

#if !BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<DownloadToolbarUIController> download_toolbar_ui_controller_;
#endif

  std::unique_ptr<ZoomBubbleCoordinator> zoom_bubble_coordinator_;

  std::unique_ptr<ActorUiWindowController> actor_ui_window_controller_;

  std::unique_ptr<ActorBorderViewController> actor_border_view_controller_;

  std::unique_ptr<BrowserSelectFileDialogController>
      browser_select_file_dialog_controller_;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  std::unique_ptr<ProfileCustomizationBubbleSyncController>
      profile_customization_bubble_sync_controller_;

  std::unique_ptr<session_restore_infobar::SessionRestoreInfobarController>
      session_restore_infobar_controller_;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  std::unique_ptr<ContextualTasksEphemeralButtonController>
      contextual_tasks_ephemeral_button_controller_;

  std::unique_ptr<tabs::GlicNudgeController> glic_nudge_controller_;

#if BUILDFLAG(ENABLE_GLIC)
  std::unique_ptr<tabs::GlicActorTaskIconController>
      glic_actor_task_icon_controller_;
  std::unique_ptr<tabs::GlicActorNudgeController> glic_actor_nudge_controller_;
  std::unique_ptr<ActorTaskListBubbleController>
      actor_task_list_bubble_controller_;
  std::unique_ptr<glic::GlicButtonController> glic_button_controller_;
  std::unique_ptr<glic::GlicIphController> glic_iph_controller_;
  std::unique_ptr<glic::GlicLegacySidePanelCoordinator>
      glic_side_panel_coordinator_;
#endif

  std::unique_ptr<contextual_tasks::ContextualTasksSidePanelCoordinator>
      contextual_tasks_side_panel_coordinator_;

  std::unique_ptr<contextual_tasks::ActiveTaskContextProvider>
      contextual_tasks_active_task_context_provider_;

  std::unique_ptr<tab_groups::MostRecentSharedTabUpdateStore>
      most_recent_shared_tab_update_store_;

  std::unique_ptr<memory_saver::MemorySaverBubbleController>
      memory_saver_bubble_controller_;

  std::unique_ptr<tab_groups::SharedTabGroupFeedbackController>
      shared_tab_group_feedback_controller_;

  std::unique_ptr<TranslateBubbleController> translate_bubble_controller_;

  std::unique_ptr<TabSearchToolbarButtonController>
      tab_search_toolbar_button_controller_;

  std::unique_ptr<CookieControlsBubbleCoordinator>
      cookie_controls_bubble_coordinator_;

  std::unique_ptr<BrowserSyncedWindowDelegate> synced_window_delegate_;

  std::unique_ptr<TabMenuModelDelegate> tab_menu_model_delegate_;

  std::unique_ptr<tab_groups::DeletionDialogController>
      tab_group_deletion_dialog_controller_;

  // Helper which implements the LocationBarModelDelegate interface.
  std::unique_ptr<BrowserLocationBarModelDelegate> location_bar_model_delegate_;

  // The model for the toolbar view.
  std::unique_ptr<LocationBarModel> location_bar_model_;

  std::unique_ptr<SigninViewController> signin_view_controller_;

  std::unique_ptr<new_tab_footer::NewTabFooterController>
      new_tab_footer_controller_;

  std::unique_ptr<DevtoolsUIController> devtools_ui_controller_;

  std::unique_ptr<enterprise_data_protection::DataProtectionUIController>
      data_protection_ui_controller_;

  std::unique_ptr<ReadingListSidePanelCoordinator>
      reading_list_side_panel_coordinator_;

  std::unique_ptr<ProfileMenuCoordinator> profile_menu_coordinator_;

  std::unique_ptr<IncognitoClearBrowsingDataDialogCoordinator>
      incognito_clear_browsing_data_dialog_coordinator_;

#if defined(USE_AURA)
  std::unique_ptr<OverscrollPrefManager> overscroll_pref_manager_;
#endif  // defined(USE_AURA)

  std::unique_ptr<ColorProviderBrowserHelper> color_provider_browser_helper_;

  // This is an experimental API that interacts with the TabStripModel.
  std::unique_ptr<TabStripServiceFeature> tab_strip_service_feature_;

  // The Find Bar. This may be NULL if there is no Find Bar, and if it is
  // non-NULL, it may or may not be visible.
  std::unique_ptr<FindBarController> find_bar_controller_;

  std::unique_ptr<DataSharingBubbleController> data_sharing_bubble_controller_;

  std::unique_ptr<TabListBridge> tab_list_bridge_;

  // Note: Depends on TabListBridge, so should come after it in the member list.
  std::unique_ptr<extensions::BrowserExtensionWindowController>
      extension_window_controller_;

  std::unique_ptr<HistoryClustersSidePanelCoordinator>
      history_clusters_side_panel_coordinator_;

  std::unique_ptr<UpgradeNotificationController>
      upgrade_notification_controller_;

  // Helper which implements the ContentSettingBubbleModel interface.
  std::unique_ptr<BrowserContentSettingBubbleModelDelegate>
      content_setting_bubble_model_delegate_;

  // Helper which implements the LiveTabContext interface.
  std::unique_ptr<BrowserLiveTabContext> live_tab_context_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  std::unique_ptr<extensions::ExtensionBrowserWindowHelper>
      extension_browser_window_helper_;
#endif

  // Listens for browser-related breadcrumb events to be added to crash reports.
  std::unique_ptr<BreadcrumbManagerBrowserAgent>
      breadcrumb_manager_browser_agent_;

  // TODO(crbug.com/423956131): Remove this.
  raw_ptr<BrowserWindowInterface> browser_ = nullptr;

  std::unique_ptr<split_tabs::SplitTabHighlightController>
      split_tab_highlight_controller_;

  std::unique_ptr<SplitViewIphController> split_view_iph_controller_;

  std::unique_ptr<RecentActivityBubbleCoordinator>
      recent_activity_bubble_coordinator_;

  std::unique_ptr<ContentsBorderController> contents_border_controller_;

#if BUILDFLAG(IS_WIN)
  std::unique_ptr<WindowsTaskbarIconUpdater> windows_taskbar_icon_updater_;
#endif

  std::unique_ptr<BrowserUserEducationInterface> user_education_;

  // TODO(webium): Current both BrowserView and WebUIBrowserWindow implement
  // AcceleratorProvider. Consider eliminating this inheritance and composing
  // this functionality into its own class.
  raw_ptr<ui::AcceleratorProvider> accelerator_provider_;

  std::unique_ptr<FindBarOwner> find_bar_owner_;

  std::unique_ptr<omnibox::AiModePageActionController>
      ai_mode_page_action_controller_;

  std::unique_ptr<SearchboxContextData> searchbox_context_data_;

  std::unique_ptr<omnibox::OmniboxPopupCloser> omnibox_popup_closer_;

  // Keep this member last to ensure embedder features are torn down first, in
  // reverse order of initialization.
  std::unique_ptr<EmbedderBrowserWindowFeatures>
      embedder_browser_window_features_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_FEATURES_H_
