// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_internals_handler.h"
#include "base/logging.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern_parser.h"
#include "privacy_sandbox_internals_handler.h"

PrivacySandboxInternalsHandler::PrivacySandboxInternalsHandler(Profile* profile)
    : profile_(profile) {}

PrivacySandboxInternalsHandler::~PrivacySandboxInternalsHandler() = default;

void PrivacySandboxInternalsHandler::GetCookieContentSettings(
    GetCookieContentSettingsCallback callback) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  std::vector<ContentSettingPatternSource> content_settings =
      map->GetSettingsForOneType(ContentSettingsType::COOKIES);

  std::move(callback).Run(std::move(content_settings));
}

void PrivacySandboxInternalsHandler::ContentSettingsPatternToString(
    const ContentSettingsPattern& pattern,
    ContentSettingsPatternToStringCallback callback) {
  std::move(callback).Run(pattern.ToString());
}
