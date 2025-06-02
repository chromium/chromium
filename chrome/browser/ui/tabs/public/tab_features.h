// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_PUBLIC_TAB_FEATURES_H_
#define CHROME_BROWSER_UI_TABS_PUBLIC_TAB_FEATURES_H_

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/buildflags.h"

class ChromeAutofillAiClient;
class FileSystemAccessPageActionController;
class FromGWSNavigationAndKeepAliveRequestObserver;
class IntentPickerViewPageActionController;
class LensOverlayController;
class LensSearchController;
class MemorySaverChipTabHelper;
class PinnedTranslateActionListener;
class Profile;
class PwaInstallPageActionController;
class ReadAnythingSidePanelController;
class SidePanelRegistry;
class TabResourceUsageTabHelper;
class TabUIHelper;
class TranslatePageActionController;

namespace commerce {
class CommerceUiTabHelper;
class PriceInsightsPageActionViewController;
}

namespace content {
class WebContents;
}  // namespace content

namespace contextual_cueing {
class ContextualCueingHelper;
}  // namespace contextual_cueing

namespace customize_chrome {
class SidePanelController;
}  // namespace customize_chrome

namespace enterprise_data_protection {
class DataProtectionNavigationController;
}  // namespace enterprise_data_protection

namespace extensions {
class ExtensionSidePanelManager;
}  // namespace extensions

#if BUILDFLAG(ENABLE_GLIC)
namespace glic {
class GlicPageContextEligibilityObserver;
class GlicTabIndicatorHelper;
}
#endif

namespace memory_saver {
class MemorySaverChipController;
}

namespace zoom {
class ZoomViewController;
}

namespace permissions {
class PermissionIndicatorsTabData;
}  // namespace permissions

namespace privacy_sandbox {
class PrivacySandboxTabObserver;
class PrivacySandboxIncognitoTabObserver;
}  // namespace privacy_sandbox

namespace metrics {
class DwaWebContentsObserver;
}  // namespace metrics

namespace sync_sessions {
class SyncSessionsRouterTabHelper;
}  // namespace sync_sessions

namespace tab_groups {
class SavedTabGroupWebContentsListener;
}  // namespace tab_groups

namespace page_actions {
class PageActionController;
}  // namespace page_actions

namespace passage_embeddings {
class EmbedderTabObserver;
}  // namespace passage_embeddings

namespace tab_groups {
class CollaborationMessagingTabData;
}  // namespace tab_groups

namespace tabs {

class TabAlertController;
class TabInterface;
class TabDialogManager;

class InactiveWindowMouseEventController;

// This class owns the core controllers for features that are scoped to a given
// tab. It can be subclassed by tests to perform dependency injection.
class TabFeatures {
 public:
  static std::unique_ptr<TabFeatures> CreateTabFeatures();
  virtual ~TabFeatures();

  TabFeatures(const TabFeatures&) = delete;
  TabFeatures& operator=(const TabFeatures&) = delete;

  // Call this method to stub out TabFeatures for tests.
  using TabFeaturesFactory =
      base::RepeatingCallback<std::unique_ptr<TabFeatures>()>;
  static void ReplaceTabFeaturesForTesting(TabFeaturesFactory factory);

  LensSearchController* lens_search_controller() {
    return lens_search_controller_.get();
  }

  enterprise_data_protection::DataProtectionNavigationController*
  data_protection_controller() {
    return data_protection_controller_.get();
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

  ChromeAutofillAiClient* chrome_autofill_ai_client() {
    return chrome_autofill_ai_client_.get();
  }

  ReadAnythingSidePanelController* read_anything_side_panel_controller() {
    return read_anything_side_panel_controller_.get();
  }

  commerce::CommerceUiTabHelper* commerce_ui_tab_helper() {
    return commerce_ui_tab_helper_.get();
  }

  privacy_sandbox::PrivacySandboxTabObserver* privacy_sandbox_tab_observer() {
    return privacy_sandbox_tab_observer_.get();
  }

  privacy_sandbox::PrivacySandboxIncognitoTabObserver*
  privacy_sandbox_incognito_tab_observer() {
    return privacy_sandbox_incognito_tab_observer_.get();
  }

  metrics::DwaWebContentsObserver* dwa_web_contents_observer() {
    return dwa_web_contents_observer_.get();
  }

  extensions::ExtensionSidePanelManager* extension_side_panel_manager() {
    return extension_side_panel_manager_.get();
  }

  tab_groups::SavedTabGroupWebContentsListener*
  saved_tab_group_web_contents_listener() const {
    return saved_tab_group_web_contents_listener_.get();
  }

  TabDialogManager* tab_dialog_manager() { return tab_dialog_manager_.get(); }

  page_actions::PageActionController* page_action_controller() {
    return page_action_controller_.get();
  }

  IntentPickerViewPageActionController*
  intent_picker_view_page_action_controller() {
    return intent_picker_view_page_action_controller_.get();
  }

  FileSystemAccessPageActionController*
  file_system_access_page_action_controller() {
    return file_system_access_page_action_controller_.get();
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

  commerce::PriceInsightsPageActionViewController*
  commerce_price_insights_page_action_view_controller() {
    return commerce_price_insights_page_action_view_controller_.get();
  }

  LensOverlayController* lens_overlay_controller();
  const LensOverlayController* lens_overlay_controller() const;

  PwaInstallPageActionController* pwa_install_page_action_controller() {
    return pwa_install_page_action_controller_.get();
  }

  InactiveWindowMouseEventController* inactive_window_mouse_event_controller() {
    return inactive_window_mouse_event_controller_.get();
  }

  TabResourceUsageTabHelper* resource_usage_helper() {
    return resource_usage_helper_.get();
  }

  MemorySaverChipTabHelper* memory_saver_chip_helper() {
    return memory_saver_chip_helper_.get();
  }

  TabUIHelper* tab_ui_helper() { return tab_ui_helper_.get(); }

  // Note: Temporary until there is a more uniform way to swap out features for
  // testing.
  TabResourceUsageTabHelper* SetResourceUsageHelperForTesting(
      std::unique_ptr<TabResourceUsageTabHelper> resource_usage_helper);

  TabUIHelper* SetTabUIHelperForTesting(
      std::unique_ptr<TabUIHelper> tab_ui_helper);

#if BUILDFLAG(ENABLE_GLIC)
  glic::GlicPageContextEligibilityObserver*
  glic_page_context_eligibility_observer() {
    return glic_page_context_eligibility_observer_.get();
  }
#endif

  TabAlertController* tab_alert_controller() {
    return tab_alert_controller_.get();
  }

  // Called exactly once to initialize features.
  // Can be overridden in tests to initialize nothing.
  virtual void Init(TabInterface& tab, Profile* profile);

 protected:
  TabFeatures();

  // Override these methods to stub out individual feature controllers for
  // testing.
  virtual std::unique_ptr<LensSearchController> CreateLensController(
      TabInterface* tab);

  virtual std::unique_ptr<commerce::CommerceUiTabHelper>
  CreateCommerceUiTabHelper(content::WebContents* web_contents,
                            Profile* profile);

 private:
  bool initialized_ = false;

  // TODO(https://crbug.com/347770670): Delete this code when tab-discarding no
  // longer swizzles WebContents.
  // Called when the tab's WebContents is discarded.
  void WillDiscardContents(tabs::TabInterface* tab,
                           content::WebContents* old_contents,
                           content::WebContents* new_contents);

  std::unique_ptr<
      enterprise_data_protection::DataProtectionNavigationController>
      data_protection_controller_;

  std::unique_ptr<permissions::PermissionIndicatorsTabData>
      permission_indicators_tab_data_;

  std::unique_ptr<SidePanelRegistry> side_panel_registry_;
  std::unique_ptr<LensSearchController> lens_search_controller_;

  // Responsible for the customize chrome tab-scoped side panel.
  std::unique_ptr<customize_chrome::SidePanelController>
      customize_chrome_side_panel_controller_;

  std::unique_ptr<ChromeAutofillAiClient> chrome_autofill_ai_client_;

  std::unique_ptr<ReadAnythingSidePanelController>
      read_anything_side_panel_controller_;

  // Responsible for commerce related features.
  std::unique_ptr<commerce::CommerceUiTabHelper> commerce_ui_tab_helper_;

  // Responsible for updating status indicator of the pinned translate button.
  std::unique_ptr<PinnedTranslateActionListener>
      pinned_translate_action_listener_;

  std::unique_ptr<privacy_sandbox::PrivacySandboxTabObserver>
      privacy_sandbox_tab_observer_;

  std::unique_ptr<privacy_sandbox::PrivacySandboxIncognitoTabObserver>
      privacy_sandbox_incognito_tab_observer_;

  std::unique_ptr<metrics::DwaWebContentsObserver>
      dwa_web_contents_observer_;

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

  // Manages various tab modal dialogs.
  std::unique_ptr<TabDialogManager> tab_dialog_manager_;

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

  // Responsible for managing the "Translate" page action.
  std::unique_ptr<TranslatePageActionController>
      translate_page_action_controller_;

  // Responsible for managing the "PWA Install" page action.
  std::unique_ptr<PwaInstallPageActionController>
      pwa_install_page_action_controller_;

  // Responsible for managing the "Zoom" page action and bubble.
  std::unique_ptr<zoom::ZoomViewController> zoom_view_controller_;

  // Responsible for managing the commerce "Price insights" page action.
  std::unique_ptr<commerce::PriceInsightsPageActionViewController>
      commerce_price_insights_page_action_view_controller_;

  // Contains the recent collaboration message for a shared tab.
  std::unique_ptr<tab_groups::CollaborationMessagingTabData>
      collaboration_messaging_tab_data_;

  std::unique_ptr<passage_embeddings::EmbedderTabObserver>
      embedder_tab_observer_;

#if BUILDFLAG(ENABLE_GLIC)
  std::unique_ptr<glic::GlicTabIndicatorHelper> glic_tab_indicator_helper_;

  std::unique_ptr<glic::GlicPageContextEligibilityObserver>
      glic_page_context_eligibility_observer_;
#endif

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

  // Must be the last member.
  base::WeakPtrFactory<TabFeatures> weak_factory_{this};
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_PUBLIC_TAB_FEATURES_H_
