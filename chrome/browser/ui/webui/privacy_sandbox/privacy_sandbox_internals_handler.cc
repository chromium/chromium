// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_internals_handler.h"
#include "base/logging.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern_parser.h"
#include "privacy_sandbox_internals_handler.h"

namespace privacy_sandbox_internals {

PrivacySandboxInternalsHandler::PrivacySandboxInternalsHandler(
    Profile* profile,
    mojo::PendingReceiver<privacy_sandbox_internals::mojom::PageHandler>
        pending_receiver)
    : profile_(profile) {
  receiver_.Bind(std::move(pending_receiver));
}

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

}  // namespace privacy_sandbox_internals
