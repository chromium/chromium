// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_PUBLIC_TAB_FEATURES_H_
#define CHROME_BROWSER_UI_TABS_PUBLIC_TAB_FEATURES_H_

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/buildflags.h"
#include "ui/base/unowned_user_data/user_data_factory.h"

class AskBeforeHttpDialogController;
class BookmarkPageActionController;
class CollaborationMessagingPageActionController;
class ContextualTasksPageActionController;
class CookieControlsPageActionController;
class FileSystemAccessPageActionController;
class FromGWSNavigationAndKeepAliveRequestObserver;
class IntentPickerViewPageActionController;
class LensOverlayController;
class LensOverlayHomeworkPageActionController;
class LensSearchController;
class MemorySaverChipTabHelper;
class PinnedTranslateActionListener;
class Profile;
class PwaInstallPageActionController;
class JsOptimizationsPageActionController;
class ReadAnythingController;
class ReadAnythingSidePanelController;
class RollBackModeBInfoBarController;
class SidePanelRegistry;
class TabResourceUsageTabHelper;
class TabUIHelper;
class TranslatePageActionController;
class QwacWebContentsObserver;
class ManagePasswordsPageActionController;
class BookmarkBarPreloadPipelineManager;
class NewTabPagePreloadPipelineManager;

namespace autofill {
class BubbleManager;
}  // namespace autofill

namespace actor {
class ActorTabData;
}  // namespace actor

namespace actor::ui {
class ActorUiTabControllerInterface;
}  // namespace actor::ui

namespace commerce {
class CommerceUiTabHelper;
class PriceInsightsPageActionViewController;
class DiscountsPageActionViewController;
}  // namespace commerce

namespace enterprise_data_protection {
class DataProtectionNavigationController;
}  // namespace enterprise_data_protection

namespace content {
class WebContents;
}  // namespace content

namespace contextual_cueing {
class ContextualCueingHelper;
}  // namespace contextual_cueing

namespace customize_chrome {
class SidePanelController;
}  // namespace customize_chrome

namespace extensions {
class ExtensionSidePanelManager;
}  // namespace extensions

#if BUILDFLAG(ENABLE_GLIC)
namespace glic {
class GlicInstanceHelper;
class GlicTabIndicatorHelper;
class GlicSidePanelCoordinator;
}  // namespace glic
#endif  // BUILDFLAG(ENABLE_GLIC)

namespace memory_saver {
class MemorySaverChipController;
}  // namespace memory_saver

namespace zoom {
class ZoomViewController;
}  // namespace zoom

namespace permissions {
class PermissionIndicatorsTabData;
}  // namespace permissions

namespace privacy_sandbox {
class PrivacySandboxTabObserver;
}  // namespace privacy_sandbox

namespace sync_sessions {
class SyncSessionsRouterTabHelper;
}  // namespace sync_sessions

namespace tab_groups {
class SavedTabGroupWebContentsListener;
class SavedTabGroupOnCloseHelper;
}  // namespace tab_groups

namespace page_actions {
class PageActionController;
}  // namespace page_actions

namespace tab_groups {
class CollaborationMessagingTabData;
}  // namespace tab_groups

namespace lens {
class TabContextualizationController;
}  // namespace lens

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
namespace wallet {
class ChromeWalletablePassClient;
}  // namespace wallet
#endif

#if BUILDFLAG(IS_CHROMEOS)
namespace web_app {
class ProtocolHandlerPickerCoordinator;
}  // namespace web_app
#endif

namespace tabs {

class TabAlertController;
class TabInterface;
class TabDialogManager;

class InactiveWindowMouseEventController;
class TabCreationMetricsController;

// This class owns the core controllers for features that are scoped to a given
// tab. It can be subclassed by tests to perform dependency injection.
class TabFeatures {
 public:
  TabFeatures();
  ~TabFeatures();

  TabFeatures(const TabFeatures&) = delete;
  TabFeatures& operator=(const TabFeatures&) = delete;

  enterprise_data_protection::DataProtectionNavigationController*
  data_protection_controller() {
    return data_protection_tab_controller_.get();
  }

  permissions::PermissionIndicatorsTabData* permission_indicators_tab_data() {
    return permission_indicators_tab_data_.get();
  }

  customize_chrome::SidePanelController*
  customize_chrome_side_panel_controller() {
    return customize_chrome_side_panel_controller_.get();
  }

  // Note: Temporary until there is a more uniform way to swap out features for
  // testing.
  customize_chrome::SidePanelController*
  SetCustomizeChromeSidePanelControllerForTesting(
      std::unique_ptr<customize_chrome::SidePanelController>
          customize_chrome_side_panel_controller);

  // This side-panel registry is tab-scoped. It is different from the browser
  // window scoped SidePanelRegistry.
  SidePanelRegistry* side_panel_registry() {
    return side_panel_registry_.get();
  }

  // TODO(crbug.com/447418049): This will be removed in the future when
  // ownership of this controller is migrated to ReadAnythingController.
  ReadAnythingSidePanelController* read_anything_side_panel_controller() {
    return read_anything_side_panel_controller_.get();
  }

  commerce::CommerceUiTabHelper* commerce_ui_tab_helper() {
    return commerce_ui_tab_helper_.get();
  }

  privacy_sandbox::PrivacySandboxTabObserver* privacy_sandbox_tab_observer() {
    return privacy_sandbox_tab_observer_.get();
  }

  extensions::ExtensionSidePanelManager* extension_side_panel_manager() {
    return extension_side_panel_manager_.get();
  }

  tab_groups::SavedTabGroupWebContentsListener*
  saved_tab_group_web_contents_listener() const {
    return saved_tab_group_web_contents_listener_.get();
  }

  tab_groups::SavedTabGroupOnCloseHelper* saved_tab_group_on_close_helper()
      const {
    return saved_tab_group_on_close_helper_.get();
  }

  TabDialogManager* tab_dialog_manager() { return tab_dialog_manager_.get(); }

  page_actions::PageActionController* page_action_controller() {
    return page_action_controller_.get();
  }

  JsOptimizationsPageActionController*
  js_optimizations_page_action_controller() {
    return js_optimizations_page_action_controller_.get();
  }

  IntentPickerViewPageActionController*
  intent_picker_view_page_action_controller() {
    return intent_picker_view_page_action_controller_.get();
  }

  FileSystemAccessPageActionController*
  file_system_access_page_action_controller() {
    return file_system_access_page_action_controller_.get();
  }

  ManagePasswordsPageActionController*
  manage_passwords_page_action_controller() {
    return manage_passwords_page_action_controller_.get();
  }

  tab_groups::CollaborationMessagingTabData*
  collaboration_messaging_tab_data() {
    return collaboration_messaging_tab_data_.get();
  }

  zoom::ZoomViewController* zoom_view_controller() {
    return zoom_view_controller_.get();
  }

  memory_saver::MemorySaverChipController* memory_saver_chip_controller() {
    return memory_saver_chip_controller_.get();
  }

  LensOverlayController* lens_overlay_controller();
  const LensOverlayController* lens_overlay_controller() const;

  lens::TabContextualizationController* tab_contextualization_controller() {
    return tab_contextualization_controller_.get();
  }

  PwaInstallPageActionController* pwa_install_page_action_controller() {
    return pwa_install_page_action_controller_.get();
  }

  InactiveWindowMouseEventController* inactive_window_mouse_event_controller() {
    return inactive_window_mouse_event_controller_.get();
  }

  MemorySaverChipTabHelper* memory_saver_chip_helper() {
    return memory_saver_chip_helper_.get();
  }

  TabUIHelper* tab_ui_helper() { return tab_ui_helper_.get(); }

  TabUIHelper* SetTabUIHelperForTesting(
      std::unique_ptr<TabUIHelper> tab_ui_helper);

  lens::TabContextualizationController*
  SetTabContextualizationControllerForTesting(
      std::unique_ptr<lens::TabContextualizationController>
          tab_contextualization_controller);

  TabCreationMetricsController* tab_creation_metrics_controller() {
    return tab_creation_metrics_controller_.get();
  }

  autofill::BubbleManager* autofill_bubble_manager() {
    return autofill_bubble_manager_.get();
  }

  autofill::BubbleManager* SetBubbleManagerForTesting(
      std::unique_ptr<autofill::BubbleManager> bubble_manager);

  AskBeforeHttpDialogController* ask_before_http_dialog_controller() {
    return ask_before_http_dialog_controller_.get();
  }

#if BUILDFLAG(ENABLE_GLIC)
  glic::GlicSidePanelCoordinator* glic_side_panel_coordinator() {
    return glic_side_panel_coordinator_.get();
  }
#endif  // BUILDFLAG(ENABLE_GLIC)

  BookmarkBarPreloadPipelineManager* bookmarkbar_preload_pipeline_manager() {
    return bookmarkbar_preload_pipeline_manager_.get();
  }

  NewTabPagePreloadPipelineManager* new_tab_page_preload_pipeline_manager() {
    return new_tab_page_preload_pipeline_manager_.get();
  }

  // Called exactly once to initialize features.
  void Init(TabInterface& tab, Profile* profile);

  static ui::UserDataFactoryWithOwner<TabInterface>&
  GetUserDataFactoryForTesting();

 private:
  bool initialized_ = false;

  // Returns the factory used to create owned components.
  static ui::UserDataFactoryWithOwner<TabInterface>& GetUserDataFactory();

  // TODO(https://crbug.com/347770670): Delete this code when tab-discarding no
  // longer swizzles WebContents.
  // Called when the tab's WebContents is discarded.
  void WillDiscardContents(tabs::TabInterface* tab,
                           content::WebContents* old_contents,
                           content::WebContents* new_contents);

  std::unique_ptr<permissions::PermissionIndicatorsTabData>
      permission_indicators_tab_data_;

  std::unique_ptr<SidePanelRegistry> side_panel_registry_;
  std::unique_ptr<LensSearchController> lens_search_controller_;

  // Responsible for the customize chrome tab-scoped side panel.
  std::unique_ptr<customize_chrome::SidePanelController>
      customize_chrome_side_panel_controller_;

  // Responsible for managing the read anything (Reading mode) feature.
  std::unique_ptr<ReadAnythingController> read_anything_controller_;

  std::unique_ptr<ReadAnythingSidePanelController>
      read_anything_side_panel_controller_;

  // Responsible for commerce related features.
  std::unique_ptr<commerce::CommerceUiTabHelper> commerce_ui_tab_helper_;

  // Responsible for updating status indicator of the pinned translate button.
  std::unique_ptr<PinnedTranslateActionListener>
      pinned_translate_action_listener_;

  std::unique_ptr<privacy_sandbox::PrivacySandboxTabObserver>
      privacy_sandbox_tab_observer_;

  // The tab-scoped extension side-panel manager. There is a separate
  // window-scoped extension side-panel manager.
  std::unique_ptr<extensions::ExtensionSidePanelManager>
      extension_side_panel_manager_;

  // Forwards tab-related events to sync.
  std::unique_ptr<sync_sessions::SyncSessionsRouterTabHelper>
      sync_sessions_router_;

  // Responsible for keeping a tab within a tab group in sync with its remote
  // tab counterpart from sync.
  std::unique_ptr<tab_groups::SavedTabGroupWebContentsListener>
      saved_tab_group_web_contents_listener_;

  std::unique_ptr<tab_groups::SavedTabGroupOnCloseHelper>
      saved_tab_group_on_close_helper_;

#if BUILDFLAG(IS_CHROMEOS)
  // Manages the protocol handler picker dialog on ChromeOS. Must be destroyed
  // after the `tab_dialog_manager_`.
  std::unique_ptr<web_app::ProtocolHandlerPickerCoordinator>
      protocol_handler_picker_coordinator_;
#endif

  // Manages various tab modal dialogs.
  std::unique_ptr<TabDialogManager> tab_dialog_manager_;

  std::unique_ptr<
      enterprise_data_protection::DataProtectionNavigationController>
      data_protection_tab_controller_;

  // Holds subscriptions for TabInterface callbacks.
  std::vector<base::CallbackListSubscription> tab_subscriptions_;

  // Responsible for managing the "Intent Picker" page action.
  std::unique_ptr<IntentPickerViewPageActionController>
      intent_picker_view_page_action_controller_;

  // Responsible for managing the "File System Access" page action.
  std::unique_ptr<FileSystemAccessPageActionController>
      file_system_access_page_action_controller_;

  // Responsible for managing all page actions of a tab. Other controllers
  // interact with this to have their feature's page action shown.
  std::unique_ptr<page_actions::PageActionController> page_action_controller_;

  // Responsible for managing the "Manage Passwords" page action.
  std::unique_ptr<ManagePasswordsPageActionController>
      manage_passwords_page_action_controller_;

  // Responsible for managing the "Translate" page action.
  std::unique_ptr<TranslatePageActionController>
      translate_page_action_controller_;

  // Responsible for managing the "PWA Install" page action.
  std::unique_ptr<PwaInstallPageActionController>
      pwa_install_page_action_controller_;

  // Responsible for managing the "Zoom" page action and bubble.
  std::unique_ptr<zoom::ZoomViewController> zoom_view_controller_;

  // Responsible for managing the "JS Optimizations" page action.
  std::unique_ptr<JsOptimizationsPageActionController>
      js_optimizations_page_action_controller_;

  // Responsible for managing the commerce "Price insights" page action.
  std::unique_ptr<commerce::PriceInsightsPageActionViewController>
      commerce_price_insights_page_action_view_controller_;

  // Responsible for managing the commerce "Price insights" page action.
  std::unique_ptr<commerce::DiscountsPageActionViewController>
      commerce_discounts_page_action_view_controller_;

  // Contains the recent collaboration message for a shared tab.
  std::unique_ptr<tab_groups::CollaborationMessagingTabData>
      collaboration_messaging_tab_data_;

  // Controller to trigger when the contextual task page action chip to
  // show/hide.
  std::unique_ptr<ContextualTasksPageActionController>
      contextual_tasks_page_action_controller_;

  // Responsible for managing the "Show Collaboration History" page action.
  std::unique_ptr<CollaborationMessagingPageActionController>
      collaboration_messaging_page_action_controller_;

  // Manages the Cookie Controls page action.
  std::unique_ptr<CookieControlsPageActionController>
      cookie_controls_page_action_controller_;

  // Manages the Lens Overlay Homework page action.
  std::unique_ptr<LensOverlayHomeworkPageActionController>
      lens_overlay_homework_page_action_controller_;

  // Manages the Bookmark page action.
  std::unique_ptr<BookmarkPageActionController>
      bookmark_page_action_controller_;

#if BUILDFLAG(ENABLE_GLIC)
  std::unique_ptr<glic::GlicInstanceHelper> glic_instance_helper_;
  std::unique_ptr<glic::GlicTabIndicatorHelper> glic_tab_indicator_helper_;
  std::unique_ptr<glic::GlicSidePanelCoordinator> glic_side_panel_coordinator_;
#endif  // BUILDFLAG(ENABLE_GLIC)

  std::unique_ptr<memory_saver::MemorySaverChipController>
      memory_saver_chip_controller_;

  std::unique_ptr<InactiveWindowMouseEventController>
      inactive_window_mouse_event_controller_;

  std::unique_ptr<FromGWSNavigationAndKeepAliveRequestObserver>
      from_gws_navigation_and_keep_alive_request_observer_;

  std::unique_ptr<TabResourceUsageTabHelper> resource_usage_helper_;

  std::unique_ptr<MemorySaverChipTabHelper> memory_saver_chip_helper_;

  std::unique_ptr<TabAlertController> tab_alert_controller_;

  std::unique_ptr<TabUIHelper> tab_ui_helper_;

  std::unique_ptr<QwacWebContentsObserver> qwac_web_contents_observer_;

  std::unique_ptr<actor::ui::ActorUiTabControllerInterface>
      actor_ui_tab_controller_;

  std::unique_ptr<TabCreationMetricsController>
      tab_creation_metrics_controller_;

  std::unique_ptr<autofill::BubbleManager> autofill_bubble_manager_;

  std::unique_ptr<AskBeforeHttpDialogController>
      ask_before_http_dialog_controller_;

  std::unique_ptr<actor::ActorTabData> actor_tab_data_;

  std::unique_ptr<lens::TabContextualizationController>
      tab_contextualization_controller_;

  std::unique_ptr<RollBackModeBInfoBarController>
      roll_back_mode_b_infobar_controller_;

  std::unique_ptr<BookmarkBarPreloadPipelineManager>
      bookmarkbar_preload_pipeline_manager_;

  std::unique_ptr<NewTabPagePreloadPipelineManager>
      new_tab_page_preload_pipeline_manager_;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<wallet::ChromeWalletablePassClient> walletable_pass_client_;
#endif
  // Must be the last member.
  base::WeakPtrFactory<TabFeatures> weak_factory_{this};
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_PUBLIC_TAB_FEATURES_H_
