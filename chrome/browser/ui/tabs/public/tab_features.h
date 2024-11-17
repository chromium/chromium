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

class ChromeAutofillAiClient;
class LensOverlayController;
class PinnedTranslateActionListener;
class Profile;
class ReadAnythingSidePanelController;
class SidePanelRegistry;

namespace commerce {
class CommerceUiTabHelper;
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

namespace permissions {
class PermissionIndicatorsTabData;
}  // namespace permissions

namespace privacy_sandbox {
class PrivacySandboxTabObserver;
}  // namespace privacy_sandbox

namespace sync_sessions {
class SyncSessionsRouterTabHelper;
}  // namespace sync_sessions

namespace tabs {

class TabInterface;

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

  LensOverlayController* lens_overlay_controller() {
    return lens_overlay_controller_.get();
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

  contextual_cueing::ContextualCueingHelper* contextual_cueing_tab_helper() {
    return contextual_cueing_helper_.get();
  }

  privacy_sandbox::PrivacySandboxTabObserver* privacy_sandbox_tab_observer() {
    return privacy_sandbox_tab_observer_.get();
  }

  extensions::ExtensionSidePanelManager* extension_side_panel_manager() {
    return extension_side_panel_manager_.get();
  }

  // Called exactly once to initialize features.
  // Can be overridden in tests to initialize nothing.
  virtual void Init(TabInterface& tab, Profile* profile);

 protected:
  TabFeatures();

  // Override these methods to stub out individual feature controllers for
  // testing.
  virtual std::unique_ptr<LensOverlayController> CreateLensController(
      TabInterface* tab,
      Profile* profile);

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
  std::unique_ptr<LensOverlayController> lens_overlay_controller_;

  // Responsible for the customize chrome tab-scoped side panel.
  std::unique_ptr<customize_chrome::SidePanelController>
      customize_chrome_side_panel_controller_;

  std::unique_ptr<ChromeAutofillAiClient> chrome_autofill_ai_client_;

  std::unique_ptr<ReadAnythingSidePanelController>
      read_anything_side_panel_controller_;

  // Responsible for commerce related features.
  std::unique_ptr<commerce::CommerceUiTabHelper> commerce_ui_tab_helper_;

  // Responsible for contextual cueing features.
  std::unique_ptr<contextual_cueing::ContextualCueingHelper>
      contextual_cueing_helper_;

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

  // Holds subscriptions for TabInterface callbacks.
  std::vector<base::CallbackListSubscription> tab_subscriptions_;

  // Must be the last member.
  base::WeakPtrFactory<TabFeatures> weak_factory_{this};
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_PUBLIC_TAB_FEATURES_H_
