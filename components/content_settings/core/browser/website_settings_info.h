// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_WEBSITE_SETTINGS_INFO_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_WEBSITE_SETTINGS_INFO_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/values.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace content_settings {

// This class stores the properties related to a website setting.
class WebsiteSettingsInfo {
 public:
  enum SyncStatus {
    // The setting will be synced.
    SYNCABLE,
    // The setting is stored locally.
    UNSYNCABLE
  };

  enum LossyStatus {
    // This marks the setting as "lossy". There is no strict time guarantee on
    // when a lossy setting will be persisted to permanent storage when it is
    // modified.
    LOSSY,
    // Changes to the setting will be persisted immediately.
    NOT_LOSSY
  };

  enum ScopingType {
    // Settings scoped to the origin of the requesting frame that can have
    // exceptions for specific top-level frame origin.
    // Use only after strongly considering if this is the right choice;
    // presenting settings that are scoped on two origins is difficult to get
    // right and often result in surprising UX.
    REQUESTING_ORIGIN_WITH_TOP_ORIGIN_EXCEPTIONS_SCOPE,

    // Settings scoped to the schemeful site of the requesting frame and the
    // top-level frame.
    // Use only after strongly considering if this is the right choice;
    // presenting settings that are scoped on two schemeful sites is difficult
    // to get right and often result in surprising UX.
    REQUESTING_AND_TOP_SCHEMEFUL_SITE_SCOPE,

    // Settings scoped to the origin of the requesting frame and the
    // schemeful site of the top-level frame.
    // Use only after strongly considering if this is the right choice;
    // presenting settings that are scoped on two schemeful sites is difficult
    // to get right and often result in surprising UX.
    REQUESTING_ORIGIN_AND_TOP_SCHEMEFUL_SITE_SCOPE,

    // Setting scoped to the origin of the requesting frame only.
    REQUESTING_ORIGIN_ONLY_SCOPE,

    // Setting scoped to the origin of the top-level frame only.
    TOP_ORIGIN_ONLY_SCOPE,

    // Setting scoped to a single origin. This does not fit into a
    // requesting/top-level origin paradigm and instead simply refers to
    // settings stored on a per-origin basis without defining which origin that
    // is. Use only when |REQUESTING_ORIGIN_ONLY_SCOPE| and
    // |TOP_ORIGIN_ONLY_SCOPE| don't apply.
    //
    // Use this comment section to keep track of cases where a better option
    // than GENERIC_SINGLE_ORIGIN_SCOPE should exist but it's not one of the
    // choices above. If the use cases are sufficient, consider adding new
    // scoping types to account for them:
    // * MEDIA_ENGAGEMENT is always scoped to the origin of the frame that a
    // video is played in. A `FRAME_ORIGIN_ONLY` scope could be considered.
    GENERIC_SINGLE_ORIGIN_SCOPE,

    // Settings scoped to the schemeful site of the requesting frame only.
    // Similar to `REQUESTING_ORIGIN_ONLY_SCOPE` but is site-scoped, not
    // origin-scoped.
    REQUESTING_SCHEMEFUL_SITE_ONLY_SCOPE,
  };

  enum IncognitoBehavior {
    // Settings will be inherited from regular to incognito profiles as usual.
    INHERIT_IN_INCOGNITO,

    // Settings will not be inherited from regular to incognito profiles.
    DONT_INHERIT_IN_INCOGNITO,
  };

  WebsiteSettingsInfo(ContentSettingsType type,
                      const std::string& name,
                      base::Value initial_default_value,
                      SyncStatus sync_status,
                      LossyStatus lossy_status,
                      ScopingType scoping_type,
                      IncognitoBehavior incognito_behavior);

  WebsiteSettingsInfo(const WebsiteSettingsInfo&) = delete;
  WebsiteSettingsInfo& operator=(const WebsiteSettingsInfo&) = delete;

  ~WebsiteSettingsInfo();

  ContentSettingsType type() const { return type_; }
  const std::string& name() const { return name_; }

  const std::string& pref_name() const { return pref_name_; }
  const std::string& partitioned_pref_name() const {
    return partitioned_pref_name_;
  }
  const std::string& default_value_pref_name() const {
    return default_value_pref_name_;
  }
  const base::Value& initial_default_value() const {
    return initial_default_value_;
  }

  uint32_t GetPrefRegistrationFlags() const;

  bool SupportsSecondaryPattern() const;

  ScopingType scoping_type() const { return scoping_type_; }
  IncognitoBehavior incognito_behavior() const { return incognito_behavior_; }

 private:
  const ContentSettingsType type_;
  const std::string name_;

  const std::string pref_name_;
  const std::string partitioned_pref_name_;
  const std::string default_value_pref_name_;
  const base::Value initial_default_value_;
  const SyncStatus sync_status_;
  const LossyStatus lossy_status_;
  const ScopingType scoping_type_;
  const IncognitoBehavior incognito_behavior_;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_WEBSITE_SETTINGS_INFO_H_
