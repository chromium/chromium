// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_customization_bubble_view.h"

#include <optional>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
#include "chrome/browser/ui/webui/signin/profile_customization_handler.h"
#include "chrome/test/supervised_user/supervision_mixin.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_education/views/help_bubble_view.h"
#include "components/supervised_user/core/browser/family_link_user_capabilities.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "content/public/test/browser_test.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class InteractiveFeatureMixinPromoTest
    : public InteractiveFeaturePromoTestT<MixinBasedInProcessBrowserTest> {
 public:
  explicit InteractiveFeatureMixinPromoTest(
      std::vector<base::test::FeatureRef> promo_features)
      : InteractiveFeaturePromoTestT<MixinBasedInProcessBrowserTest>(
            UseDefaultTrackerAllowingPromos(promo_features)) {}
};

// TODO(crbug.com/40274192): Rename the file to match the class once
// `ProfileCustomizationBubbleView` is deleted.
class ProfileCustomizationBrowserTest
    : public InteractiveFeatureMixinPromoTest,
      public testing::WithParamInterface<
          supervised_user::SupervisionMixin::SignInMode> {
 public:
  ProfileCustomizationBrowserTest()
      : InteractiveFeatureMixinPromoTest(
            {feature_engagement::kIPHProfileSwitchFeature,
             feature_engagement::kIPHSupervisedUserProfileSigninFeature}) {}

 protected:
  supervised_user::SupervisionMixin::SignInMode GetSupervisionSignInMode() {
    return GetParam();
  }

  bool IsSupervisedUser() {
    return GetSupervisionSignInMode() ==
                supervised_user::SupervisionMixin::SignInMode::kSupervised;
  }

 private:
  supervised_user::SupervisionMixin supervision_mixin_{
      mixin_host_,
      this,
      embedded_test_server(),
      {.sign_in_mode = GetSupervisionSignInMode()}};
};

IN_PROC_BROWSER_TEST_P(ProfileCustomizationBrowserTest, IPH) {
  auto scoped_iph_delay =
      AvatarToolbarButton::SetScopedIPHMinDelayAfterCreationForTesting(
          base::Seconds(0));

  ASSERT_EQ(IsSupervisedUser(),
            supervised_user::IsPrimaryAccountSubjectToParentalControls(
                IdentityManagerFactory::GetForProfile(browser()->profile())) ==
                signin::Tribool::kTrue);

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
      WaitForPromo(
          // If the user is supervised, the supervised user profile IPH is
          // shown. Otherwise, the profile switch IPH is shown.
          IsSupervisedUser()
              ? feature_engagement::kIPHSupervisedUserProfileSigninFeature
              : feature_engagement::kIPHProfileSwitchFeature),
      PressClosePromoButton());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ProfileCustomizationBrowserTest,
    testing::Values(supervised_user::SupervisionMixin::SignInMode::kSupervised,
                    supervised_user::SupervisionMixin::SignInMode::kRegular),
    [](const auto& info) {
      return info.param ==
                     supervised_user::SupervisionMixin::SignInMode::kSupervised
                 ? "ForSupervisedUser"
                 : "ForRegularUser";
    });
