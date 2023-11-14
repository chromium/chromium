// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SAFETY_HUB_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SAFETY_HUB_HANDLER_H_

#include <set>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/clock.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "url/origin.h"

// The state of Safe Browsing settings.
enum class SafeBrowsingState {
  kEnabledEnhanced = 0,
  kEnabledStandard = 1,
  kDisabledByAdmin = 2,
  kDisabledByExtension = 3,
  kDisabledByUser = 4,
  // New enum values must go above here.
  kMaxValue = kDisabledByUser,
};

/**
 * This handler deals with the permission-related operations on the site
 * settings page.
 */

class SafetyHubHandler : public settings::SettingsPageUIHandler {
 public:
  enum class SafetyHubModule {
    kExtensions,
    kNotifications,
    kPasswords,
    kSafeBrowsing,
    kUnusedSitePermissions,
    kVersion
  };

  explicit SafetyHubHandler(Profile* profile);

  ~SafetyHubHandler() override;

  static std::unique_ptr<SafetyHubHandler> GetForProfile(Profile* profile);

 private:
  friend class SafetyHubHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(SafetyHubHandlerTest,
                           PopulateUnusedSitePermissionsData);
  FRIEND_TEST_ALL_PREFIXES(SafetyHubHandlerTest,
                           HandleAllowPermissionsAgainForUnusedSite);
  FRIEND_TEST_ALL_PREFIXES(SafetyHubHandlerTest,
                           HandleAcknowledgeRevokedUnusedSitePermissionsList);
  FRIEND_TEST_ALL_PREFIXES(SafetyHubHandlerTest,
                           HandleIgnoreOriginsForNotificationPermissionReview);
  FRIEND_TEST_ALL_PREFIXES(SafetyHubHandlerTest,
                           HandleBlockNotificationPermissionForOrigins);
  FRIEND_TEST_ALL_PREFIXES(SafetyHubHandlerTest,
                           HandleAllowNotificationPermissionForOrigins);
  FRIEND_TEST_ALL_PREFIXES(SafetyHubHandlerTest,
                           HandleResetNotificationPermissionForOrigins);
  FRIEND_TEST_ALL_PREFIXES(SafetyHubHandlerTest,
                           PopulateNotificationPermissionReviewData);
  FRIEND_TEST_ALL_PREFIXES(
      SafetyHubHandlerTest,
      HandleUndoIgnoreOriginsForNotificationPermissionReview);
  FRIEND_TEST_ALL_PREFIXES(SafetyHubHandlerTest,
                           SendNotificationPermissionReviewList_FeatureEnabled);
  FRIEND_TEST_ALL_PREFIXES(
      SafetyHubHandlerTest,
      SendNotificationPermissionReviewList_FeatureDisabled);
  FRIEND_TEST_ALL_PREFIXES(SafetyHubHandlerTest, RevokeAllContentSettingTypes);
  FRIEND_TEST_ALL_PREFIXES(SafetyHubHandlerParameterizedTest,
                           PasswordCardState);
  FRIEND_TEST_ALL_PREFIXES(SafetyHubHandlerTest, PasswordCardCheckTime);
  FRIEND_TEST_ALL_PREFIXES(SafetyHubHandlerTest, VersionCardUpToDate);
  FRIEND_TEST_ALL_PREFIXES(SafetyHubHandlerTest, VersionCardOutOfDate);

  // SettingsPageUIHandler implementation.
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Returns the list of revoked permissions to be used in
  // "Unused site permissions" module.
  void HandleGetRevokedUnusedSitePermissionsList(const base::Value::List& args);

  // Re-grant the revoked permissions and remove the given origin from the
  // revoked permissions list.
  void HandleAllowPermissionsAgainForUnusedSite(const base::Value::List& args);

  // Reverse the changes made by |HandleAllowPermissionsAgainForUnusedSite| for
  // the given |UnusedSitePermission| object.
  void HandleUndoAllowPermissionsAgainForUnusedSite(
      const base::Value::List& args);

  // Clear the list of revoked permissions so they are not shown again.
  // Permission settings themselves are not affected by this.
  void HandleAcknowledgeRevokedUnusedSitePermissionsList(
      const base::Value::List& args);

  // Reverse the changes made by
  // |HandleAcknowledgeRevokedUnusedSitePermissionsList| for the given list of
  // |UnusedSitePermission| objects. List of revoked
  // permissions is repopulated. Permission settings are not changed.
  void HandleUndoAcknowledgeRevokedUnusedSitePermissionsList(
      const base::Value::List& args);

  // Returns the list of revoked permissions that belongs to origins which
  // haven't been visited recently.
  // TODO(crbug.com/1443466): Get list of revoked permissions from the unused
  // site permission service instead.
  base::Value::List PopulateUnusedSitePermissionsData();

  // Sends the list of unused site permissions to review to the WebUI.
  void SendUnusedSitePermissionsReviewList();

  // Returns the list of notification permissions that needs to be reviewed.
  void HandleGetNotificationPermissionReviewList(const base::Value::List& args);

  // Handles ignoring origins for the review notification permissions feature.
  void HandleIgnoreOriginsForNotificationPermissionReview(
      const base::Value::List& args);

  // Handles resetting a notification permission for given origins.
  void HandleResetNotificationPermissionForOrigins(
      const base::Value::List& args);

  // Handles blocking notification permissions for multiple origins.
  void HandleBlockNotificationPermissionForOrigins(
      const base::Value::List& args);

  // Handles allowing notification permissions for multiple origins.
  void HandleAllowNotificationPermissionForOrigins(
      const base::Value::List& args);

  // Handles reverting the action of ignoring origins for review notification
  // permissions feature by removing them from the notification permission
  // verification blocklist.
  void HandleUndoIgnoreOriginsForNotificationPermissionReview(
      const base::Value::List& args);

  // Handles dismissing the active menu notification for Safety Hub.
  void HandleDismissActiveMenuNotification(const base::Value::List& args);

  // Handles dismissing the menu notifications for the password module.
  void HandleDismissPasswordMenuNotification(const base::Value::List& args);

  // Returns the data for Safe Browsing card.
  void HandleGetSafeBrowsingCardData(const base::Value::List& args);

  // Fetches data for the Safe Browsing card to return data to the UI.
  base::Value::Dict GetSafeBrowsingCardData();

  // Returns the data for the password card.
  void HandleGetPasswordCardData(const base::Value::List& args);

  // Fetches data for the password card to return data to the UI.
  base::Value::Dict GetPasswordCardData();

  // Returns the data for the version card.
  void HandleGetVersionCardData(const base::Value::List& args);

  // Fetches data for the version card to return data to the UI.
  base::Value::Dict GetVersionCardData();

  // Returns true if Safety Hub has recommendations.
  void HandleGetSafetyHubHasRecommendations(const base::Value::List& args);

  // Returns the subheader for Safety Hub entry point in settings.
  void HandleGetSafetyHubEntryPointSubheader(const base::Value::List& args);

  // Sends the list of notification permissions to review to the WebUI.
  void SendNotificationPermissionReviewList();

  // Returns the number of extensions that should be reviewed by the user.
  int GetNumberOfExtensionsThatNeedReview();

  // Returns the set of Safety Hub modules which require the user's attention.
  std::set<SafetyHubModule> GetSafetyHubModulesWithRecommendations();

  const raw_ptr<Profile, DanglingUntriaged> profile_;

  raw_ptr<base::Clock> clock_;

  void SetClockForTesting(base::Clock* clock);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SAFETY_HUB_HANDLER_H_
