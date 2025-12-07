// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_PERMISSION_SETTINGS_INFO_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_PERMISSION_SETTINGS_INFO_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/content_settings/core/common/content_settings.h"

namespace base {
class Value;
}

namespace content_settings {

class WebsiteSettingsInfo;

class PermissionSettingsInfo {
 public:
  // Implements PermissionSetting specific logic like validation, incognito
  // inherince and parsing.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Return whether the setting is valid.
    virtual bool IsValid(const PermissionSetting& setting) const = 0;
    virtual bool IsDefaultSettingValid(
        const PermissionSetting& setting) const = 0;

    // Returns a setting to inherit to incognito mode.
    virtual PermissionSetting InheritInIncognito(
        const PermissionSetting& setting) const = 0;

    // Returns if at least some of the permission setting is allowed. Used e.g.
    // to decide whether the permission setting can be auto-revoked by
    // SafetyHub.
    virtual bool IsAnyPermissionAllowed(
        const PermissionSetting& setting) const = 0;

    // Returns true when no permission has been allowed or blocked yet.
    virtual bool IsUndecided(const PermissionSetting& setting) const = 0;

    // Returns whether the permission is fully blocked. This is usually the case
    // when nothing is allowed and the permission is not undecided.
    virtual bool IsBlocked(const PermissionSetting& setting) const;

    // Returns whether the permission setting supports expiration tracking.
    virtual bool CanTrackLastVisit() const = 0;

    // Returns true if any existing persistent state should be coalesced with
    // ephemeral state from the OneTimePermissionProvider.
    virtual bool ShouldCoalesceEphemeralState() const = 0;

    // Returns a PermissionSetting that represents the permission when under
    // permission embargo. E.g. turns ASK into BLOCK.
    virtual PermissionSetting ApplyPermissionEmbargo(
        const PermissionSetting& setting) const = 0;

    // Returns the coalesced PermissionSetting based on the passed in persistent
    // and ephemeral state.
    virtual PermissionSetting CoalesceEphemeralState(
        const PermissionSetting& persistent_permission_setting,
        const PermissionSetting& ephemeral_permission_setting) const;

    // Parsing and conversion methods.
    virtual base::Value ToValue(const PermissionSetting& setting) const = 0;
    virtual std::optional<PermissionSetting> FromValue(
        const base::Value& value) const = 0;
  };

  enum OriginRestriction {
    // This flag indicates content types that only allow exceptions to be set
    // on secure origins.
    EXCEPTIONS_ON_SECURE_ORIGINS_ONLY,
    // This flag indicates content types that allow exceptions to be set on
    // secure and insecure origins.
    EXCEPTIONS_ON_SECURE_AND_INSECURE_ORIGINS,
  };

  // This object does not take ownership of |website_settings_info|.
  PermissionSettingsInfo(
      const WebsiteSettingsInfo* website_settings_info,
      const std::vector<std::string>& allowlisted_primary_schemes,
      OriginRestriction origin_restriction,
      std::unique_ptr<Delegate> delegate);

  PermissionSettingsInfo(const PermissionSettingsInfo&) = delete;
  PermissionSettingsInfo& operator=(const PermissionSettingsInfo&) = delete;

  ~PermissionSettingsInfo();

  const WebsiteSettingsInfo* website_settings_info() const {
    return website_settings_info_;
  }
  const std::vector<std::string>& allowlisted_primary_schemes() const {
    return allowlisted_primary_schemes_;
  }

  // Gets the original default setting for a particular content type.
  PermissionSetting GetInitialDefaultSetting() const;

  OriginRestriction origin_restriction() const { return origin_restriction_; }

  const Delegate& delegate() const { return *delegate_; }

 private:
  raw_ptr<const WebsiteSettingsInfo> website_settings_info_;
  const std::vector<std::string> allowlisted_primary_schemes_;
  const OriginRestriction origin_restriction_;
  const std::unique_ptr<Delegate> delegate_;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_PERMISSION_SETTINGS_INFO_H_
