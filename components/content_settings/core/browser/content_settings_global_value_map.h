// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_GLOBAL_VALUE_MAP_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_GLOBAL_VALUE_MAP_H_

#include <map>

#include "components/content_settings/core/browser/content_settings_provider.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace content_settings {

class RuleIterator;

// A simplified value map that sets global content settings, i.e. applying to
// all sites.
// Note that this class does not do any synchronization. As content settings are
// accessed from multiple threads, it's the responsibility of the client to
// prevent concurrent access.
class GlobalValueMap {
 public:
  GlobalValueMap();

  GlobalValueMap(const GlobalValueMap&) = delete;
  GlobalValueMap& operator=(const GlobalValueMap&) = delete;

  ~GlobalValueMap();

  // Returns nullptr to indicate the RuleIterator is empty.
  std::unique_ptr<RuleIterator> GetRuleIterator(
      ContentSettingsType content_type) const;
  void SetContentSetting(ContentSettingsType content_type,
                         ContentSetting setting);
  ContentSetting GetContentSetting(ContentSettingsType content_type) const;

 private:
  std::map<ContentSettingsType, ContentSetting> settings_;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_GLOBAL_VALUE_MAP_H_
