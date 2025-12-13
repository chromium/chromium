// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_REVOKED_PERMISSIONS_RESULT_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_REVOKED_PERMISSIONS_RESULT_H_

#include <list>
#include <map>
#include <set>

#include "chrome/browser/ui/safety_hub/safety_hub_result.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"

inline constexpr char kRevokedPermissionsResultKey[] = "permissions";

struct ContentSettingEntry {
  ContentSettingsType type;
  ContentSettingPatternSource source;
};

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.safety_hub
// LINT.IfChange(PermissionsRevocationType)
enum class PermissionsRevocationType {
  kUnusedPermissions,
  kAbusiveNotificationPermissions,
  kDisruptiveNotificationPermissions,
  kUnusedPermissionsAndAbusiveNotifications,
  kUnusedPermissionsAndDisruptiveNotifications,
  kSuspiciousNotificationPermissions,
  kUnusedPermissionsAndSuspiciousNotifications,
};
// LINT.ThenChange(//chrome/browser/resources/settings/safety_hub/safety_hub_browser_proxy.ts:PermissionsRevocationType)

// Class to store data about unused permissions for a given origin.
struct PermissionsData {
 public:
  PermissionsData();
  ~PermissionsData();
  PermissionsData(const PermissionsData&);
  PermissionsData& operator=(const PermissionsData&) = delete;

  ContentSettingsPattern primary_pattern;
  std::set<ContentSettingsType> permission_types;
  base::Value::Dict chooser_permissions_data;
  content_settings::ContentSettingConstraints constraints;
  PermissionsRevocationType revocation_type;
};

class RevokedPermissionsResult : public SafetyHubResult {
 public:
  RevokedPermissionsResult();

  RevokedPermissionsResult(const RevokedPermissionsResult&);
  RevokedPermissionsResult& operator=(const RevokedPermissionsResult&) =
      default;

  ~RevokedPermissionsResult() override;

  using UnusedPermissionMap =
      std::map<std::string, std::list<ContentSettingEntry>>;

  // Adds a revoked permission, defined by origin, a set of permission types
  // and the expiration until the user is made aware of the revoked
  // permission.
  void AddRevokedPermission(PermissionsData);

  void SetRecentlyUnusedPermissions(UnusedPermissionMap map) {
    recently_unused_permissions_ = std::move(map);
  }

  const UnusedPermissionMap& GetRecentlyUnusedPermissions() {
    return recently_unused_permissions_;
  }

  const std::list<PermissionsData>& GetRevokedPermissions();

  std::set<ContentSettingsPattern> GetRevokedOrigins() const;

  // SafetyHubResult implementation
  base::Value::Dict ToDictValue() const override;
  bool IsTriggerForMenuNotification() const override;
  bool WarrantsNewMenuNotification(
      const base::Value::Dict& previous_result_dict) const override;
  std::u16string GetNotificationString() const override;
  int GetNotificationCommandId() const override;
  std::unique_ptr<SafetyHubResult> Clone() const override;

 private:
  std::list<PermissionsData> revoked_permissions_;
  UnusedPermissionMap recently_unused_permissions_;
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_REVOKED_PERMISSIONS_RESULT_H_
