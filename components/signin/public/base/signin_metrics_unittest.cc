// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_metrics.h"

#include <string>
#include <vector>

#include "base/stl_util.h"
#include "base/test/metrics/user_action_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin_metrics {

namespace {

const AccessPoint kAccessPointsThatSupportImpression[] = {
    AccessPoint::ACCESS_POINT_START_PAGE,
    AccessPoint::ACCESS_POINT_NTP_LINK,
    AccessPoint::ACCESS_POINT_MENU,
    AccessPoint::ACCESS_POINT_SETTINGS,
    AccessPoint::ACCESS_POINT_EXTENSION_INSTALL_BUBBLE,
    AccessPoint::ACCESS_POINT_APPS_PAGE_LINK,
    AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE,
    AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER,
    AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN,
    AccessPoint::ACCESS_POINT_DEVICES_PAGE,
    AccessPoint::ACCESS_POINT_CLOUD_PRINT,
    AccessPoint::ACCESS_POINT_SIGNIN_PROMO,
    AccessPoint::ACCESS_POINT_RECENT_TABS,
    AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE,
    AccessPoint::ACCESS_POINT_AUTOFILL_DROPDOWN,
    AccessPoint::ACCESS_POINT_NTP_CONTENT_SUGGESTIONS,
    AccessPoint::ACCESS_POINT_RESIGNIN_INFOBAR,
    AccessPoint::ACCESS_POINT_TAB_SWITCHER,
    AccessPoint::ACCESS_POINT_SAVE_CARD_BUBBLE,
    AccessPoint::ACCESS_POINT_MANAGE_CARDS_BUBBLE};

const AccessPoint kAccessPointsThatSupportPersonalizedPromos[] = {
    AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER,
    AccessPoint::ACCESS_POINT_RECENT_TABS,
    AccessPoint::ACCESS_POINT_SETTINGS,
    AccessPoint::ACCESS_POINT_TAB_SWITCHER,
    AccessPoint::ACCESS_POINT_EXTENSION_INSTALL_BUBBLE,
    AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN,
    AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE,
    AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE,
    AccessPoint::ACCESS_POINT_NTP_CONTENT_SUGGESTIONS,
    AccessPoint::ACCESS_POINT_SAVE_CARD_BUBBLE,
    AccessPoint::ACCESS_POINT_MANAGE_CARDS_BUBBLE};

class SigninMetricsTest : public ::testing::Test {
 public:
  static std::string GetAccessPointDescription(AccessPoint access_point) {
    switch (access_point) {
      case AccessPoint::ACCESS_POINT_START_PAGE:
        return "StartPage";
      case AccessPoint::ACCESS_POINT_NTP_LINK:
        return "NTP";
      case AccessPoint::ACCESS_POINT_MENU:
        return "Menu";
      case AccessPoint::ACCESS_POINT_SETTINGS:
        return "Settings";
      case AccessPoint::ACCESS_POINT_SUPERVISED_USER:
        return "SupervisedUser";
      case AccessPoint::ACCESS_POINT_EXTENSION_INSTALL_BUBBLE:
        return "ExtensionInstallBubble";
      case AccessPoint::ACCESS_POINT_EXTENSIONS:
        return "Extensions";
      case AccessPoint::ACCESS_POINT_APPS_PAGE_LINK:
        return "AppsPageLink";
      case AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE:
        return "BookmarkBubble";
      case AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER:
        return "BookmarkManager";
      case AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN:
        return "AvatarBubbleSignin";
      case AccessPoint::ACCESS_POINT_USER_MANAGER:
        return "UserManager";
      case AccessPoint::ACCESS_POINT_DEVICES_PAGE:
        return "DevicesPage";
      case AccessPoint::ACCESS_POINT_CLOUD_PRINT:
        return "CloudPrint";
      case AccessPoint::ACCESS_POINT_CONTENT_AREA:
        return "ContentArea";
      case AccessPoint::ACCESS_POINT_SIGNIN_PROMO:
        return "SigninPromo";
      case AccessPoint::ACCESS_POINT_RECENT_TABS:
        return "RecentTabs";
      case AccessPoint::ACCESS_POINT_UNKNOWN:
        return "UnknownAccessPoint";
      case AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE:
        return "PasswordBubble";
      case AccessPoint::ACCESS_POINT_AUTOFILL_DROPDOWN:
        return "AutofillDropdown";
      case AccessPoint::ACCESS_POINT_NTP_CONTENT_SUGGESTIONS:
        return "NTPContentSuggestions";
      case AccessPoint::ACCESS_POINT_RESIGNIN_INFOBAR:
        return "ReSigninInfobar";
      case AccessPoint::ACCESS_POINT_TAB_SWITCHER:
        return "TabSwitcher";
      case AccessPoint::ACCESS_POINT_SAVE_CARD_BUBBLE:
        return "SaveCardBubble";
      case AccessPoint::ACCESS_POINT_MANAGE_CARDS_BUBBLE:
        return "ManageCardsBubble";
      case AccessPoint::ACCESS_POINT_MACHINE_LOGON:
        return "MachineLogon";
      case AccessPoint::ACCESS_POINT_GOOGLE_SERVICES_SETTINGS:
        return "GoogleServicesSettings";
      case AccessPoint::ACCESS_POINT_MAX:
        NOTREACHED();
        return "";
    }
    NOTREACHED();
    return "";
  }

  static std::vector<AccessPoint> GetAllAccessPoints() {
    std::vector<AccessPoint> access_points;
    for (int ap = 0; ap < static_cast<int>(AccessPoint::ACCESS_POINT_MAX);
         ++ap) {
      // Skip the deprecated ACCESS_POINT_FORCE_SIGNIN_WARNING
      if (ap == 23)
        continue;
      access_points.push_back(static_cast<AccessPoint>(ap));
    }
    return access_points;
  }

  static bool AccessPointSupportsPersonalizedPromo(AccessPoint access_point) {
    return base::Contains(kAccessPointsThatSupportPersonalizedPromos,
                          access_point);
  }
};

TEST_F(SigninMetricsTest, RecordSigninUserActionForAccessPoint) {
  for (const AccessPoint& ap : GetAllAccessPoints()) {
    base::UserActionTester user_action_tester;
    RecordSigninUserActionForAccessPoint(
        ap, signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Signin_Signin_From" + GetAccessPointDescription(ap)));
  }
}

TEST_F(SigninMetricsTest, RecordSigninUserActionWithPromoAction) {
  for (const AccessPoint& ap : kAccessPointsThatSupportPersonalizedPromos) {
    {
      // PROMO_ACTION_WITH_DEFAULT promo action
      base::UserActionTester user_action_tester;
      RecordSigninUserActionForAccessPoint(
          ap, signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT);
      EXPECT_EQ(
          1, user_action_tester.GetActionCount("Signin_SigninWithDefault_From" +
                                               GetAccessPointDescription(ap)));
    }
    {
      // PROMO_ACTION_NOT_DEFAULT promo action
      base::UserActionTester user_action_tester;
      RecordSigninUserActionForAccessPoint(
          ap, signin_metrics::PromoAction::PROMO_ACTION_NOT_DEFAULT);
      EXPECT_EQ(
          1, user_action_tester.GetActionCount("Signin_SigninNotDefault_From" +
                                               GetAccessPointDescription(ap)));
    }
  }
}

TEST_F(SigninMetricsTest, RecordSigninUserActionWithNewPreDicePromoAction) {
  for (const AccessPoint& ap : GetAllAccessPoints()) {
    base::UserActionTester user_action_tester;
    RecordSigninUserActionForAccessPoint(
        ap, signin_metrics::PromoAction::PROMO_ACTION_NEW_ACCOUNT_PRE_DICE);
    if (AccessPointSupportsPersonalizedPromo(ap)) {
      EXPECT_EQ(1, user_action_tester.GetActionCount(
                       "Signin_SigninNewAccountPreDice_From" +
                       GetAccessPointDescription(ap)));
    } else {
      EXPECT_EQ(0, user_action_tester.GetActionCount(
                       "Signin_SigninNewAccountPreDice_From" +
                       GetAccessPointDescription(ap)));
    }
  }
}

TEST_F(SigninMetricsTest, RecordSigninUserActionWithNewNoExistingPromoAction) {
  for (const AccessPoint& ap : GetAllAccessPoints()) {
    base::UserActionTester user_action_tester;
    RecordSigninUserActionForAccessPoint(
        ap, signin_metrics::PromoAction::
                PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT);
    if (AccessPointSupportsPersonalizedPromo(ap)) {
      EXPECT_EQ(1, user_action_tester.GetActionCount(
                       "Signin_SigninNewAccountNoExistingAccount_From" +
                       GetAccessPointDescription(ap)));
    } else {
      EXPECT_EQ(0, user_action_tester.GetActionCount(
                       "Signin_SigninNewAccountNoExistingAccount_From" +
                       GetAccessPointDescription(ap)));
    }
  }
}

TEST_F(SigninMetricsTest,
       RecordSigninUserActionWithNewWithExistingPromoAction) {
  for (const AccessPoint& ap : GetAllAccessPoints()) {
    base::UserActionTester user_action_tester;
    RecordSigninUserActionForAccessPoint(
        ap,
        signin_metrics::PromoAction::PROMO_ACTION_NEW_ACCOUNT_EXISTING_ACCOUNT);
    if (AccessPointSupportsPersonalizedPromo(ap)) {
      EXPECT_EQ(1, user_action_tester.GetActionCount(
                       "Signin_SigninNewAccountExistingAccount_From" +
                       GetAccessPointDescription(ap)));
    } else {
      EXPECT_EQ(0, user_action_tester.GetActionCount(
                       "Signin_SigninNewAccountExistingAccount_From" +
                       GetAccessPointDescription(ap)));
    }
  }
}

TEST_F(SigninMetricsTest, RecordSigninImpressionUserAction) {
  for (const AccessPoint& ap : kAccessPointsThatSupportImpression) {
    base::UserActionTester user_action_tester;
    RecordSigninImpressionUserActionForAccessPoint(ap);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Signin_Impression_From" + GetAccessPointDescription(ap)));
  }
}

TEST_F(SigninMetricsTest, RecordSigninImpressionWithAccountUserAction) {
  for (const AccessPoint& ap : kAccessPointsThatSupportPersonalizedPromos) {
    base::UserActionTester user_action_tester;
    RecordSigninImpressionWithAccountUserActionForAccessPoint(ap, true);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Signin_ImpressionWithAccount_From" +
                     GetAccessPointDescription(ap)));
  }
}

TEST_F(SigninMetricsTest, RecordSigninImpressionWithNoAccountUserAction) {
  for (const AccessPoint& ap : kAccessPointsThatSupportPersonalizedPromos) {
    base::UserActionTester user_action_tester;
    RecordSigninImpressionWithAccountUserActionForAccessPoint(ap, false);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Signin_ImpressionWithNoAccount_From" +
                     GetAccessPointDescription(ap)));
  }
}

}  // namespace
}  // namespace signin_metrics
