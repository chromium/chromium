// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/content_settings/content_settings_handler.h"

#include <utility>

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings_pattern_parser.h"
#include "mojo/public/cpp/bindings/message.h"

namespace content_settings_internals {

using ::content_settings::WebsiteSettingsRegistry;

ContentSettingsHandler::ContentSettingsHandler(
    Profile* profile,
    mojo::PendingReceiver<content_settings_internals::mojom::PageHandler>
        pending_receiver)
    : profile_(profile) {
  receiver_.Bind(std::move(pending_receiver));
}

ContentSettingsHandler::~ContentSettingsHandler() = default;

void ContentSettingsHandler::ReadContentSettings(
    const ContentSettingsType type,
    ReadContentSettingsCallback callback) {
  if (!IsKnownEnumValue(type)) {
    mojo::ReportBadMessage(
        "ReadContentSettings received invalid ContentSettingsType");
    return;
  }

  // HostContentSettingsMap will assert if we attempt to read unregistered
  // content types, so for these types we simply return an empty list.
  if (WebsiteSettingsRegistry::GetInstance()->Get(type) == nullptr) {
    std::move(callback).Run({});
    return;
  }

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  std::move(callback).Run(map->GetSettingsForOneType(type));
}

void ContentSettingsHandler::ContentSettingsPatternToString(
    const ContentSettingsPattern& pattern,
    ContentSettingsPatternToStringCallback callback) {
  std::move(callback).Run(pattern.ToString());
}

void ContentSettingsHandler::StringToContentSettingsPattern(
    const std::string& s,
    StringToContentSettingsPatternCallback callback) {
  std::move(callback).Run(ContentSettingsPattern::FromString(s));
}

}  // namespace content_settings_internals
