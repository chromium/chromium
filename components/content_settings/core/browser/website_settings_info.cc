// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/website_settings_info.h"

#include <utility>

#include "base/check.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry.h"

namespace {

const char kPrefPrefix[] = "profile.content_settings.exceptions.";
const char kPartitionedPrefPrefix[] =
    "profile.content_settings.partitioned_exceptions.";
const char kDefaultPrefPrefix[] = "profile.default_content_setting_values.";

std::string GetPreferenceName(const std::string& name, const char* prefix) {
  std::string pref_name = name;
  base::ReplaceChars(pref_name, "-", "_", &pref_name);
  return std::string(prefix).append(pref_name);
}

}  // namespace

namespace content_settings {

WebsiteSettingsInfo::WebsiteSettingsInfo(ContentSettingsType type,
                                         const std::string& name,
                                         base::Value initial_default_value,
                                         SyncStatus sync_status,
                                         LossyStatus lossy_status,
                                         ScopingType scoping_type,
                                         IncognitoBehavior incognito_behavior)
    : type_(type),
      name_(name),
      pref_name_(GetPreferenceName(name, kPrefPrefix)),
      partitioned_pref_name_(GetPreferenceName(name, kPartitionedPrefPrefix)),
      default_value_pref_name_(GetPreferenceName(name, kDefaultPrefPrefix)),
      initial_default_value_(std::move(initial_default_value)),
      sync_status_(sync_status),
      lossy_status_(lossy_status),
      scoping_type_(scoping_type),
      incognito_behavior_(incognito_behavior) {
  // For legacy reasons the default value is currently restricted to be an int
  // or none.
  // TODO(raymes): We should migrate the underlying pref to be a dictionary
  // rather than an int.
  DCHECK(initial_default_value_.is_none() || initial_default_value_.is_int());
}

WebsiteSettingsInfo::~WebsiteSettingsInfo() = default;

uint32_t WebsiteSettingsInfo::GetPrefRegistrationFlags() const {
  uint32_t flags = PrefRegistry::NO_REGISTRATION_FLAGS;

  if (sync_status_ == SYNCABLE)
    flags |= user_prefs::PrefRegistrySyncable::SYNCABLE_PREF;

  if (lossy_status_ == LOSSY)
    flags |= PrefRegistry::LOSSY_PREF;

  return flags;
}

bool WebsiteSettingsInfo::SupportsSecondaryPattern() const {
  switch (scoping_type_) {
    case REQUESTING_ORIGIN_WITH_TOP_ORIGIN_EXCEPTIONS_SCOPE:
    case REQUESTING_AND_TOP_SCHEMEFUL_SITE_SCOPE:
    case REQUESTING_ORIGIN_AND_TOP_SCHEMEFUL_SITE_SCOPE:
      return true;
    case REQUESTING_ORIGIN_ONLY_SCOPE:
    case TOP_ORIGIN_ONLY_SCOPE:
    case GENERIC_SINGLE_ORIGIN_SCOPE:
    case REQUESTING_SCHEMEFUL_SITE_ONLY_SCOPE:
      return false;
  }
}

}  // namespace content_settings
