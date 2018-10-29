// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/content_settings.h"

#include <algorithm>
#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/content_settings_utils.h"

namespace {

struct HistogramValue {
  ContentSettingsType type;
  int value;
};

// WARNING: The value specified here for a type should match exactly the value
// specified in the ContentType enum in enums.xml. Since these values are
// used for histograms, please do not reuse the same value for a different
// content setting. Always append to the end and increment.
//
// TODO(raymes): We should use a sparse histogram here on the hash of the
// content settings type name instead.
//
// The array size must be explicit for the static_asserts below.
constexpr size_t kNumHistogramValues = 39;
constexpr HistogramValue kHistogramValue[kNumHistogramValues] = {
    {CONTENT_SETTINGS_TYPE_COOKIES, 0},
    {CONTENT_SETTINGS_TYPE_IMAGES, 1},
    {CONTENT_SETTINGS_TYPE_JAVASCRIPT, 2},
    {CONTENT_SETTINGS_TYPE_PLUGINS, 3},
    {CONTENT_SETTINGS_TYPE_POPUPS, 4},
    {CONTENT_SETTINGS_TYPE_GEOLOCATION, 5},
    {CONTENT_SETTINGS_TYPE_NOTIFICATIONS, 6},
    {CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE, 7},
    {CONTENT_SETTINGS_TYPE_MIXEDSCRIPT, 10},
    {CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, 12},
    {CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, 13},
    {CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS, 14},
    {CONTENT_SETTINGS_TYPE_PPAPI_BROKER, 15},
    {CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS, 16},
    {CONTENT_SETTINGS_TYPE_MIDI_SYSEX, 17},
    {CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS, 19},
    {CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER, 21},
    {CONTENT_SETTINGS_TYPE_APP_BANNER, 22},
    {CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, 23},
    {CONTENT_SETTINGS_TYPE_DURABLE_STORAGE, 24},
    {CONTENT_SETTINGS_TYPE_BLUETOOTH_GUARD, 26},
    {CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC, 27},
    {CONTENT_SETTINGS_TYPE_AUTOPLAY, 28},
    {CONTENT_SETTINGS_TYPE_IMPORTANT_SITE_INFO, 30},
    {CONTENT_SETTINGS_TYPE_PERMISSION_AUTOBLOCKER_DATA, 31},
    {CONTENT_SETTINGS_TYPE_ADS, 32},
    {CONTENT_SETTINGS_TYPE_ADS_DATA, 33},
    {CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION, 34},
    {CONTENT_SETTINGS_TYPE_MEDIA_ENGAGEMENT, 35},
    {CONTENT_SETTINGS_TYPE_SOUND, 36},
    {CONTENT_SETTINGS_TYPE_CLIENT_HINTS, 37},
    {CONTENT_SETTINGS_TYPE_SENSORS, 38},
    {CONTENT_SETTINGS_TYPE_ACCESSIBILITY_EVENTS, 39},
    {CONTENT_SETTINGS_TYPE_CLIPBOARD_READ, 40},
    {CONTENT_SETTINGS_TYPE_CLIPBOARD_WRITE, 41},
    {CONTENT_SETTINGS_TYPE_PLUGINS_DATA, 42},
    {CONTENT_SETTINGS_TYPE_PAYMENT_HANDLER, 43},
    {CONTENT_SETTINGS_TYPE_USB_GUARD, 44},
    {CONTENT_SETTINGS_TYPE_BACKGROUND_FETCH, 45},
};

}  // namespace

ContentSetting IntToContentSetting(int content_setting) {
  return ((content_setting < 0) ||
          (content_setting >= CONTENT_SETTING_NUM_SETTINGS))
             ? CONTENT_SETTING_DEFAULT
             : static_cast<ContentSetting>(content_setting);
}

int ContentSettingTypeToHistogramValue(ContentSettingsType content_setting,
                                       size_t* num_values) {
  *num_values = arraysize(kHistogramValue);

  // Verify the array is sorted by enum type and contains all values.
  DCHECK(std::is_sorted(std::begin(kHistogramValue), std::end(kHistogramValue),
                        [](const HistogramValue& a, const HistogramValue& b) {
                          return a.type < b.type;
                        }));
  static_assert(kHistogramValue[kNumHistogramValues - 1].type ==
                    CONTENT_SETTINGS_NUM_TYPES - 1,
                "Update content settings histogram lookup");

  const HistogramValue* found = std::lower_bound(
      std::begin(kHistogramValue), std::end(kHistogramValue), content_setting,
      [](const HistogramValue& a, ContentSettingsType b) {
        return a.type < b;
      });
  if (found != std::end(kHistogramValue) && found->type == content_setting)
    return found->value;
  NOTREACHED();
  return -1;
}

ContentSettingPatternSource::ContentSettingPatternSource(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    base::Value setting_value,
    const std::string& source,
    bool incognito)
    : primary_pattern(primary_pattern),
      secondary_pattern(secondary_pattern),
      setting_value(std::move(setting_value)),
      source(source),
      incognito(incognito) {}

ContentSettingPatternSource::ContentSettingPatternSource() : incognito(false) {}

ContentSettingPatternSource::ContentSettingPatternSource(
    const ContentSettingPatternSource& other) {
  *this = other;
}

ContentSettingPatternSource& ContentSettingPatternSource::operator=(
    const ContentSettingPatternSource& other) {
  primary_pattern = other.primary_pattern;
  secondary_pattern = other.secondary_pattern;
  setting_value = other.setting_value.Clone();
  source = other.source;
  incognito = other.incognito;
  return *this;
}

ContentSettingPatternSource::~ContentSettingPatternSource() {}

ContentSetting ContentSettingPatternSource::GetContentSetting() const {
  return content_settings::ValueToContentSetting(&setting_value);
}

// static
bool RendererContentSettingRules::IsRendererContentSetting(
    ContentSettingsType content_type) {
  return content_type == CONTENT_SETTINGS_TYPE_IMAGES ||
         content_type == CONTENT_SETTINGS_TYPE_JAVASCRIPT ||
         content_type == CONTENT_SETTINGS_TYPE_AUTOPLAY ||
         content_type == CONTENT_SETTINGS_TYPE_CLIENT_HINTS ||
         content_type == CONTENT_SETTINGS_TYPE_POPUPS;
}

RendererContentSettingRules::RendererContentSettingRules() {}

RendererContentSettingRules::~RendererContentSettingRules() {}
