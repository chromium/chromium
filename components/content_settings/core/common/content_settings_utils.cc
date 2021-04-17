// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/content_settings_utils.h"

#include <memory>

#include "base/values.h"

namespace content_settings {

namespace {

// Converts a |Value| to a |ContentSetting|. Returns true if |value| encodes
// a valid content setting, false otherwise. Note that |CONTENT_SETTING_DEFAULT|
// is encoded as a NULL value, so it is not allowed as an integer value.
bool ParseContentSettingValue(const base::Value* value,
                              ContentSetting* setting) {
  if (!value) {
    *setting = CONTENT_SETTING_DEFAULT;
    return true;
  }
  if (!value->is_int())
    return false;
  *setting = IntToContentSetting(value->GetInt());
  return *setting != CONTENT_SETTING_DEFAULT;
}

}  // namespace

ContentSetting ValueToContentSetting(const base::Value* value) {
  ContentSetting setting = CONTENT_SETTING_DEFAULT;
  bool valid = ParseContentSettingValue(value, &setting);
  DCHECK(valid);
  return setting;
}

std::unique_ptr<base::Value> ContentSettingToValue(ContentSetting setting) {
  if (setting <= CONTENT_SETTING_DEFAULT ||
      setting >= CONTENT_SETTING_NUM_SETTINGS) {
    return nullptr;
  }
  return std::make_unique<base::Value>(setting);
}

}  // namespace content_settings
