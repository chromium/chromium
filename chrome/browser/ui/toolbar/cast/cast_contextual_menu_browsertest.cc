// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button_menu_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/media_router/common/pref_names.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "ui/actions/actions.h"

class CastContextualMenuBrowserTest : public InProcessBrowserTest {
 public:
  CastContextualMenuBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kPinnedCastButton);
  }

  void SetUpOnMainThread() override {
    // Pin the Cast icon to the toolbar.
    PinnedToolbarActionsModel::Get(browser()->profile())
        ->UpdatePinnedState(kActionRouteMedia, true);
  }

  void SetUpInProcessBrowserTestFixture() override {
    provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnectorBase::SetPolicyProviderForTesting(&provider_);
  }

  policy::MockConfigurationPolicyProvider* provider() { return &provider_; }

 protected:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests the basic state of the contextual menu.
IN_PROC_BROWSER_TEST_F(CastContextualMenuBrowserTest, Basic) {
  // About
  // Learn more
  // Help
  // Optimize fullscreen videos (checkbox)
  // Report an issue
  // -----
  // Pin
  // Unpin
  // Customize Toolbar

  // Number of menu items, including separators.
  size_t expected_number_items = 8;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  expected_number_items += 1;
#endif

  PinnedActionToolbarButtonMenuModel model(browser(), kActionRouteMedia);
  EXPECT_EQ(model.GetItemCount(), expected_number_items);

  // Verify all cast specific items are enabled and visible.
  for (size_t i = 0; i < expected_number_items - 4; ++i) {
    EXPECT_TRUE(model.IsEnabledAt(i));
    EXPECT_TRUE(model.IsVisibleAt(i));
  }
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// "Report an issue" should be present for normal profiles as well as for
// incognito.
IN_PROC_BROWSER_TEST_F(CastContextualMenuBrowserTest,
                       EnableAndDisableReportIssue) {
  PinnedActionToolbarButtonMenuModel model(browser(), kActionRouteMedia);
  std::vector<actions::ActionId> model_actions;
  for (size_t index = 0; index < model.GetItemCount(); index++) {
    model_actions.push_back(model.GetActionIdAtForTesting(index));
  }
  EXPECT_TRUE(
      base::Contains(model_actions, kActionMediaToolbarContextReportCastIssue));

  Browser* incognito_browser = CreateIncognitoBrowser(browser()->profile());

  PinnedActionToolbarButtonMenuModel incognito_menu(incognito_browser,
                                                    kActionRouteMedia);
  std::vector<actions::ActionId> incognito_model_actions;
  for (size_t index = 0; index < model.GetItemCount(); index++) {
    incognito_model_actions.push_back(model.GetActionIdAtForTesting(index));
  }
  EXPECT_TRUE(base::Contains(incognito_model_actions,
                             kActionMediaToolbarContextReportCastIssue));
}
#endif

IN_PROC_BROWSER_TEST_F(CastContextualMenuBrowserTest, ToggleMediaRemotingItem) {
  PinnedActionToolbarButtonMenuModel model(browser(), kActionRouteMedia);
  int remoting_index = -1;
  for (size_t index = 0; index < model.GetItemCount(); index++) {
    if (model.GetActionIdAtForTesting(index) ==
        kActionMediaRouterToggleMediaRemoting) {
      remoting_index = index;
    }
  }
  EXPECT_NE(remoting_index, -1);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->SetBoolean(
      media_router::prefs::kMediaRouterMediaRemotingEnabled, false);
  EXPECT_FALSE(model.IsItemCheckedAt(remoting_index));

  model.ActivatedAt(remoting_index);
  EXPECT_TRUE(model.IsItemCheckedAt(remoting_index));
  EXPECT_TRUE(pref_service->GetBoolean(
      media_router::prefs::kMediaRouterMediaRemotingEnabled));

  model.ActivatedAt(remoting_index);
  EXPECT_FALSE(model.IsItemCheckedAt(remoting_index));
  EXPECT_FALSE(pref_service->GetBoolean(
      media_router::prefs::kMediaRouterMediaRemotingEnabled));
}

IN_PROC_BROWSER_TEST_F(CastContextualMenuBrowserTest,
                       PinUnpinItemRespectsPolicyPref) {
  PinnedToolbarActionsModel::Get(browser()->profile())
      ->UpdatePinnedState(kActionRouteMedia, false);
  // Set cast to be pinned based on policy.
  policy::PolicyMap policy_map;
  policy_map.Set(policy::key::kShowCastIconInToolbar,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  provider()->UpdateChromePolicy(policy_map);

  PinnedActionToolbarButtonMenuModel model(browser(), kActionRouteMedia);
  int pin_index = -1;
  int unpin_index = -1;
  for (size_t index = 0; index < model.GetItemCount(); index++) {
    if (model.GetActionIdAtForTesting(index) == kActionPinActionToToolbar) {
      pin_index = index;
    }
    if (model.GetActionIdAtForTesting(index) == kActionUnpinActionFromToolbar) {
      unpin_index = index;
    }
  }
  EXPECT_NE(pin_index, -1);
  EXPECT_NE(unpin_index, -1);

  // All pin options should be disabled and 'unpin' should be the visible
  // disabled option.
  EXPECT_FALSE(model.IsEnabledAt(pin_index));
  EXPECT_FALSE(model.IsEnabledAt(unpin_index));
  EXPECT_FALSE(model.IsVisibleAt(pin_index));
  EXPECT_TRUE(model.IsVisibleAt(unpin_index));
}
