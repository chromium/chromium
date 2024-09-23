// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_customization_bubble_view.h"

#include <optional>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
#include "chrome/browser/ui/webui/signin/profile_customization_handler.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

// TODO(crbug.com/40274192): Rename the file to match the class once
// `ProfileCustomizationBubbleView` is deleted.
class ProfileCustomizationBrowserTest : public InteractiveFeaturePromoTest {
 public:
  ProfileCustomizationBrowserTest()
      : InteractiveFeaturePromoTest(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHProfileSwitchFeature})) {}
};

IN_PROC_BROWSER_TEST_F(ProfileCustomizationBrowserTest, IPH) {
  AvatarToolbarButton::SetIPHMinDelayAfterCreationForTesting(base::Seconds(0));

  RunTestSequence(
      WithView(kToolbarAvatarButtonElementId,
               [this](AvatarToolbarButton* button) {
                 ProfileCustomizationBubbleView::CreateBubble(browser(),
                                                              button);
               }),
      Check(
          [this]() {
            return browser()->signin_view_controller()->ShowsModalDialog();
          },
          "Check modal dialog showing."),
      EnsureNotPresent(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      Check(
          [this]() {
            return login_ui_test_utils::CompleteProfileCustomizationDialog(
                browser());
          },
          "Finish customization."),
      WaitForPromo(feature_engagement::kIPHProfileSwitchFeature),
      PressClosePromoButton());
}
