// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_DETAILS_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_DETAILS_H_

#include <string>

#include "base/macros.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"

// Details for the CONTENT_SETTINGS_CHANGED notification. This is sent when
// content settings change for at least one host. If settings change for more
// than one pattern in one user interaction, this will usually send a single
// notification with update_all() returning true instead of one notification
// for each pattern.
class ContentSettingsDetails {
 public:
  // Update the setting that matches this pattern/content type/resource.
  ContentSettingsDetails(const ContentSettingsPattern& primary_pattern,
                         const ContentSettingsPattern& secondary_pattern,
                         ContentSettingsType type,
                         const std::string& resource_identifier);

  // The item pattern whose settings have changed.
  const ContentSettingsPattern& primary_pattern() const {
    return primary_pattern_;
  }

  // The top level frame pattern whose settings have changed.
  const ContentSettingsPattern& secondary_pattern() const {
    return secondary_pattern_;
  }

  // True if all settings should be updated for the given type.
  bool update_all() const {
    return primary_pattern_.ToString().empty() &&
           secondary_pattern_.ToString().empty();
  }

  // The type of the pattern whose settings have changed.
  ContentSettingsType type() const { return type_; }

  // The resource identifier for the settings type that has changed.
  const std::string& resource_identifier() const {
    return resource_identifier_;
  }

  // True if all types should be updated. If update_all() is false, this will
  // be false as well (although the reverse does not hold true).
  bool update_all_types() const {
    return ContentSettingsType::DEFAULT == type_;
  }

 private:
  ContentSettingsPattern primary_pattern_;
  ContentSettingsPattern secondary_pattern_;
  ContentSettingsType type_;
  std::string resource_identifier_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingsDetails);
};

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_DETAILS_H_
