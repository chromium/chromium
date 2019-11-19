// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_UTILS_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_UTILS_H_

#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"

class HostContentSettingsMap;

namespace content_settings {

typedef std::pair<ContentSettingsPattern, ContentSettingsPattern> PatternPair;

// Helper class to iterate over only the values in a map.
template <typename IteratorType, typename ReferenceType>
class MapValueIterator {
 public:
  explicit MapValueIterator(IteratorType iterator) : iterator_(iterator) {}

  bool operator!=(const MapValueIterator& other) const {
    return iterator_ != other.iterator_;
  }

  MapValueIterator& operator++() {
    ++iterator_;
    return *this;
  }

  ReferenceType operator*() { return iterator_->second.get(); }

 private:
  IteratorType iterator_;
};

// These constants are copied from extensions/common/extension_constants.h and
// content/public/common/url_constants.h to avoid complicated dependencies.
const char kChromeDevToolsScheme[] = "devtools";
const char kChromeUIScheme[] = "chrome";
const char kExtensionScheme[] = "chrome-extension";

std::string ContentSettingToString(ContentSetting setting);

// Converts a content setting string to the corresponding ContentSetting.
// Returns true if |name| specifies a valid content setting, false otherwise.
bool ContentSettingFromString(const std::string& name, ContentSetting* setting);

PatternPair ParsePatternString(const std::string& pattern_str);

std::string CreatePatternString(
    const ContentSettingsPattern& item_pattern,
    const ContentSettingsPattern& top_level_frame_pattern);

// Populates |rules| with content setting rules for content types that are
// handled by the renderer.
void GetRendererContentSettingRules(const HostContentSettingsMap* map,
                                    RendererContentSettingRules* rules);

// Returns true if setting |a| is more permissive than setting |b|.
bool IsMorePermissive(ContentSetting a, ContentSetting b);

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_UTILS_H_
