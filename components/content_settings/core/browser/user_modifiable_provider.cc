// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/user_modifiable_provider.h"

namespace content_settings {

void UserModifiableProvider::ExpireWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_settings_type) {
  SetWebsiteSetting(primary_pattern, secondary_pattern, content_settings_type,
                    base::Value(), {});
}

void UserModifiableProvider::EnsureUpdatedSettings(base::OnceClosure callback) {
  std::move(callback).Run();
}

}  // namespace content_settings
