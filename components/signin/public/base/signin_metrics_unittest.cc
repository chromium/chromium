// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_metrics.h"

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin_metrics {

namespace {

const AccessPoint kAccessPointsThatSupportUserAction[] = {
    AccessPoint::kStartPage,
    AccessPoint::kMenu,
    AccessPoint::kSettings,
    AccessPoint::kSettingsYourSavedInfo,
    AccessPoint::kExtensionInstallBubble,
    AccessPoint::kExtensions,
    AccessPoint::kBookmarkBubble,
    AccessPoint::kBookmarkManager,
    AccessPoint::kAvatarBubbleSignIn,
    AccessPoint::kUserManager,
    AccessPoint::kFullscreenSigninPromo,
    AccessPoint::kRecentTabs,
    AccessPoint::kUnknown,
    AccessPoint::kPasswordBubble,
    AccessPoint::kAutofillDropdown,
    AccessPoint::kResigninInfobar,
    AccessPoint::kMachineLogon,
    AccessPoint::kNtpFeedTopPromo,
    AccessPoint::kPostDeviceRestoreSigninPromo,
    AccessPoint::kNtpFeedCardMenuPromo,
    AccessPoint::kNtpFeedBottomPromo,
    AccessPoint::kCreatorFeedFollow,
    AccessPoint::kReadingList,
    AccessPoint::kSetUpList,
    AccessPoint::kChromeSigninInterceptBubble,
    AccessPoint::kTabOrganization,
    AccessPoint::kNotificationsOptInScreenContentToggle,
    AccessPoint::kAvatarBubbleSignInWithSyncPromo,
    AccessPoint::kProductSpecifications,
    AccessPoint::kAddressBubble,
    AccessPoint::kGlicLaunchButton,
    AccessPoint::kNonModalSigninPasswordPromo,
    AccessPoint::kNonModalSigninBookmarkPromo,
    AccessPoint::kUserManagerWithPrefilledEmail,
    AccessPoint::kEnterpriseDialogAfterSigninInterception,
    AccessPoint::kCredentialExchangeImport,
};

const AccessPoint kAccessPointsThatSupportImpression[] = {
    AccessPoint::kStartPage,
    AccessPoint::kMenu,
    AccessPoint::kSettings,
    AccessPoint::kSettingsYourSavedInfo,
    AccessPoint::kExtensionInstallBubble,
    AccessPoint::kBookmarkBubble,
    AccessPoint::kBookmarkManager,
    AccessPoint::kAvatarBubbleSignIn,
    AccessPoint::kFullscreenSigninPromo,
    AccessPoint::kRecentTabs,
    AccessPoint::kPasswordBubble,
    AccessPoint::kAutofillDropdown,
    AccessPoint::kResigninInfobar,
    AccessPoint::kNtpFeedTopPromo,
    AccessPoint::kPostDeviceRestoreSigninPromo,
    AccessPoint::kNtpFeedCardMenuPromo,
    AccessPoint::kNtpFeedBottomPromo,
    AccessPoint::kCreatorFeedFollow,
    AccessPoint::kReadingList,
    AccessPoint::kSetUpList,
    AccessPoint::kChromeSigninInterceptBubble,
    AccessPoint::kNotificationsOptInScreenContentToggle,
    AccessPoint::kAddressBubble,
    AccessPoint::kEnterpriseDialogAfterSigninInterception,
    AccessPoint::kCredentialExchangeImport,
};

class SigninMetricsTest : public ::testing::Test {
 public:
  static std::string GetAccessPointDescription(AccessPoint access_point) {
    switch (access_point) {
      case AccessPoint::kStartPage:
        return "StartPage";
      case AccessPoint::kMenu:
        return "Menu";
      case AccessPoint::kSettings:
        return "Settings";
      case AccessPoint::kSettingsYourSavedInfo:
        return "YourSavedInfo";
      case AccessPoint::kExtensionInstallBubble:
        return "ExtensionInstallBubble";
      case AccessPoint::kExtensions:
        return "Extensions";
      case AccessPoint::kBookmarkBubble:
        return "BookmarkBubble";
      case AccessPoint::kBookmarkManager:
        return "BookmarkManager";
      case AccessPoint::kAvatarBubbleSignIn:
        return "AvatarBubbleSignin";
      case AccessPoint::kUserManager:
        return "UserManager";
      case AccessPoint::kFullscreenSigninPromo:
        return "SigninPromo";
      case AccessPoint::kRecentTabs:
        return "RecentTabs";
      case AccessPoint::kUnknown:
        return "UnknownAccessPoint";
      case AccessPoint::kPasswordBubble:
        return "PasswordBubble";
      case AccessPoint::kAutofillDropdown:
        return "AutofillDropdown";
      case AccessPoint::kResigninInfobar:
        return "ReSigninInfobar";
      case AccessPoint::kMachineLogon:
        return "MachineLogon";
      case AccessPoint::kForcedSignin:
        return "ForcedSignin";
      case AccessPoint::kWebSignin:
        return "WebSignIn";
      case AccessPoint::kSafetyCheck:
        return "SafetyCheck";
      case AccessPoint::kSigninInterceptFirstRunExperience:
        return "SigninInterceptFirstRunExperience";
      case AccessPoint::kSendTabToSelfPromo:
        return "SendTabToSelfPromo";
      case AccessPoint::kNtpFeedTopPromo:
        return "NTPFeedTopPromo";
      case AccessPoint::kSettingsSyncOffRow:
        return "SettingsSyncOffRow";
      case AccessPoint::kPostDeviceRestoreSigninPromo:
        return "PostDeviceRestoreSigninPromo";
      case AccessPoint::kPostDeviceRestoreBackgroundSignin:
        return "PostDeviceRestoreBackgroundSignin";
      case AccessPoint::kNtpSignedOutIcon:
        return "NTPSignedOutIcon";
      case AccessPoint::kNtpFeedCardMenuPromo:
        return "NTPFeedCardMenuSigninPromo";
      case AccessPoint::kNtpFeedBottomPromo:
        return "NTPFeedBottomSigninPromo";
      case AccessPoint::kDesktopSigninManager:
        return "DesktopSigninManager";
      case AccessPoint::kForYouFre:
        return "ForYouFre";
      case AccessPoint::kCreatorFeedFollow:
        return "CreatorFeedFollow";
      case AccessPoint::kReadingList:
        return "ReadingList";
      case AccessPoint::kReauthInfoBar:
        return "ReauthInfoBar";
      case AccessPoint::kAccountConsistencyService:
        return "AccountConsistencyService";
      case AccessPoint::kSetUpList:
        return "SetUpList";
      case AccessPoint::kSaveToDriveIos:
        return "SaveToDrive";
      case AccessPoint::kSaveToPhotosIos:
        return "SaveToPhotos";
      case AccessPoint::kChromeSigninInterceptBubble:
        return "ChromeSigninInterceptBubble";
      case AccessPoint::kRestorePrimaryAccountOnProfileLoad:
        return "RestorePrimaryAccountinfoOnProfileLoad";
      case AccessPoint::kTabOrganization:
        return "TabOrganization";
      case AccessPoint::kTipsNotification:
        return "TipsNotification";
      case AccessPoint::kNotificationsOptInScreenContentToggle:
        return "NotificationsOptInScreenContentToggle";
      case AccessPoint::kSigninChoiceRemembered:
        return "SigninChoiceRemembered";
      case AccessPoint::kProfileMenuSignoutConfirmationPrompt:
        return "ProfileMenuSignoutConfirmationPrompt";
      case AccessPoint::kSettingsSignoutConfirmationPrompt:
        return "SettingsSignoutConfirmationPrompt";
      case AccessPoint::kOidcRedirectionInterception:
        return "OidcRedirectionInterception";
      case AccessPoint::kWebauthnModalDialog:
        return "WebAuthnModalDialog";
      case AccessPoint::kAvatarBubbleSignInWithSyncPromo:
        return "AvatarBubbleSigninWithSyncPromo";
      case AccessPoint::kAccountMenuSwitchAccount:
        return "AccountMenu";
      case AccessPoint::kAccountMenuSwitchAccountFailed:
        return "AccountMenuFailedSwitch";
      case AccessPoint::kProductSpecifications:
        return "ProductSpecifications";
      case AccessPoint::kAddressBubble:
        return "AddressBubble";
      case AccessPoint::kCctAccountMismatchNotification:
        return "CctAccountMismatchNotification";
      case AccessPoint::kDriveFilePickerIos:
        return "DriveFilePickerIOS";
      case AccessPoint::kCollaborationShareTabGroup:
        return "CollaborationShareTabGroup";
      case AccessPoint::kGlicLaunchButton:
        return "GlicLaunchButton";
      case AccessPoint::kHistoryPage:
        return "HistoryPage";
      case AccessPoint::kCollaborationJoinTabGroup:
        return "CollaborationJoinTabGroup";
      case AccessPoint::kHistorySyncOptinExpansionPillOnStartup:
        return "HistorySyncOptinExpansionPillOnStartup";
      case AccessPoint::kWidget:
        return "Widget";
      case AccessPoint::kCollaborationLeaveOrDeleteTabGroup:
        return "CollaborationLeaveOrDeleteTabGroup";
      case AccessPoint::kHistorySyncEducationalTip:
        return "HistorySyncEducationalTip";
      case AccessPoint::kManagedProfileAutoSigninIos:
        return "ManagedProfileAutoSigninIos";
      case AccessPoint::kNonModalSigninPasswordPromo:
        return "NonModalSigninPasswordPromo";
      case AccessPoint::kNonModalSigninBookmarkPromo:
        return "NonModalSigninBookmarkPromo";
      case AccessPoint::kUserManagerWithPrefilledEmail:
        return "UserManagerWithPrefilledEmail";
      case AccessPoint::kEnterpriseManagementDisclaimerAtStartup:
        return "EnterpriseManagementDisclaimerAtStartup";
      case AccessPoint::kEnterpriseManagementDisclaimerAfterBrowserFocus:
        return "EnterpriseManagementDisclaimerAfterBrowserFocus";
      case AccessPoint::kEnterpriseManagementDisclaimerAfterSignin:
        return "EnterpriseManagementDisclaimerAfterSignin";
      case AccessPoint::kNtpFeaturePromo:
        return "NtpFeaturePromo";
      case AccessPoint::kEnterpriseDialogAfterSigninInterception:
        return "EnterpriseDialogAfterSigninInterception";
      case AccessPoint::kCredentialExchangeImport:
        return "CredentialExchangeImport";
      case AccessPoint::kSetSyncConsentFromSyncInternals:
        return "SetSyncConsentFromSyncInternals";
      case AccessPoint::kIosChromeWebView:
        return "IosChromeWebView";
      case AccessPoint::kAshChromeSessionManager:
        return "AshChromeSessionManager";
      case AccessPoint::kAshUserSessionManager:
        return "AshUserSessionManager";
    }
  }
};

TEST_F(SigninMetricsTest, RecordSigninUserActionForAccessPoint) {
  for (const AccessPoint& ap : kAccessPointsThatSupportUserAction) {
    base::UserActionTester user_action_tester;
    RecordSigninUserActionForAccessPoint(ap);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Signin_Signin_From" + GetAccessPointDescription(ap)));
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

TEST_F(SigninMetricsTest, AccessPointFromInt) {
  EXPECT_EQ(AccessPoint::kStartPage, AccessPointFromInt(0));
  EXPECT_EQ(std::optional<AccessPoint>(), AccessPointFromInt(-1));
  // Deprecated access point kNtpLink.
  EXPECT_EQ(std::optional<AccessPoint>(), AccessPointFromInt(1));
}

TEST(LogSyncOptInOfferedTest, RecordsHistogram) {
  base::HistogramTester histogram_tester;
  const AccessPoint access_point =
      AccessPoint::kHistorySyncOptinExpansionPillOnStartup;
  LogSyncOptInOffered(access_point);
  LogSyncOptInOffered(access_point);
  histogram_tester.ExpectUniqueSample("Signin.SyncOptIn.Offered", access_point,
                                      /*expected_bucket_count=*/2);
}

}  // namespace
}  // namespace signin_metrics
