// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tpcd/metadata/manager.h"

#include <cstdint>
#include <random>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/hash/sha1.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_enums.mojom-shared.h"
#include "components/content_settings/core/common/content_settings_rules.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/host_indexed_content_settings.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/tpcd/metadata/metadata.pb.h"
#include "components/tpcd/metadata/parser.h"
#include "components/tpcd/metadata/prefs.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace tpcd::metadata {

namespace {
uint32_t ElectedDtrp(const MetadataEntry& metadata_entry) {
  return metadata_entry.has_dtrp_override() ? metadata_entry.dtrp_override()
                                            : metadata_entry.dtrp();
}
}  // namespace

// static
Manager* Manager::GetInstance(Parser* parser,
                              GrantsSyncCallback callback,
                              PrefService* local_state) {
  static base::NoDestructor<Manager> instance(parser, std::move(callback),
                                              local_state);
  return instance.get();
}

Manager::Manager(Parser* parser,
                 GrantsSyncCallback callback,
                 PrefService* local_state)
    : parser_(parser),
      grants_sync_callback_(std::move(callback)),
      local_state_(local_state) {
  CHECK(parser_);

  if (base::FeatureList::IsEnabled(
          content_settings::features::kHostIndexedMetadataGrants)) {
    base::AutoLock lock(grants_lock_);
    grants_ = content_settings::HostIndexedContentSettings();
  } else {
    base::AutoLock lock(grants_lock_);
    grants_ = ContentSettingsForOneType();
  }

  parser_->AddObserver(this);
  if (!parser_->GetMetadata().empty()) {
    OnMetadataReady();
  }
}

Manager::~Manager() {
  parser_->RemoveObserver(this);
}

bool Manager::IsAllowed(const GURL& url,
                        const GURL& first_party_url,
                        content_settings::SettingInfo* out_info) const {
  base::AutoLock lock(grants_lock_);
  return CONTENT_SETTING_ALLOW ==
         GetContentSetting(grants_, url, first_party_url, out_info);
}

uint32_t Manager::GenerateRand() const {
  base::RandomBitGenerator generator;
  std::uniform_int_distribution<uint32_t> distribution(Parser::kMinDtrp + 1,
                                                       Parser::kMaxDtrp);
  uint32_t rand = distribution(generator);
  return rand;
}

std::string Manager::GenerateKeyHash(
    const MetadataEntry& metadata_entry) const {
  std::string key = base::StrCat(
      {metadata_entry.primary_pattern_spec(),
       /*Non-url delimiter:*/ "\\", metadata_entry.secondary_pattern_spec(),
       base::NumberToString(ElectedDtrp(metadata_entry))});

  // The hash from SHA1 is faster to obtain than SHA256 and will be forever
  // unchanged if key is unchanged and provides lesser collision risk than MD5.
  return base::Base64Encode(base::SHA1HashString(key));
}

void Manager::SetGrants(const ContentSettingsForOneType& grants) {
  if (base::FeatureList::IsEnabled(
          content_settings::features::kHostIndexedMetadataGrants)) {
    auto indices = content_settings::HostIndexedContentSettings::Create(grants);
    if (indices.empty()) {
      base::AutoLock lock(grants_lock_);
      grants_ = content_settings::HostIndexedContentSettings();
    } else {
      CHECK_EQ(indices.size(), 1u);
      base::AutoLock lock(grants_lock_);
      grants_ = std::move(indices.front());
    }
  } else {
    base::AutoLock lock(grants_lock_);
    grants_ = grants;
  }

  if (grants_sync_callback_) {
    grants_sync_callback_.Run(grants);
  }
}

void Manager::OnMetadataReady() {
  if (!base::FeatureList::IsEnabled(net::features::kTpcdMetadataGrants)) {
    return;
  }

  base::flat_set<std::string> remove_keys;
  if (base::FeatureList::IsEnabled(
          net::features::kTpcdMetadataStagedRollback) &&
      local_state_) {
    const base::Value::Dict& dict = local_state_->GetDict(prefs::kCohorts);
    for (const auto itr : dict) {
      remove_keys.insert(itr.first);
    }
  }

  ContentSettingsForOneType grants;
  for (const auto& metadata_entry : parser_->GetMetadata()) {
    const auto primary_pattern = ContentSettingsPattern::FromString(
        metadata_entry.primary_pattern_spec());
    const auto secondary_pattern = ContentSettingsPattern::FromString(
        metadata_entry.secondary_pattern_spec());

    // This is unlikely to occurred as it is validated before the component is
    // installed by the component installer.
    if (!primary_pattern.IsValid() || !secondary_pattern.IsValid()) {
      continue;
    }

    base::Value value(ContentSetting::CONTENT_SETTING_ALLOW);

    content_settings::RuleMetaData rule_metadata =
        content_settings::RuleMetaData();
    rule_metadata.set_tpcd_metadata_rule_source(
        Parser::ToRuleSource(metadata_entry.source()));

    std::optional<content_settings::mojom::TpcdMetadataCohort> cohort;

    if (!Parser::IsDtrpEligible(
            Parser::ToRuleSource(metadata_entry.source())) ||
        !base::FeatureList::IsEnabled(
            net::features::kTpcdMetadataStagedRollback)) {
      cohort = content_settings::mojom::TpcdMetadataCohort::DEFAULT;
    }

    std::string key_hash = GenerateKeyHash(metadata_entry);

    // Get the cohort from the prefs if available.
    if (!cohort.has_value()) {
      if (local_state_) {
        const base::Value::Dict& dict = local_state_->GetDict(prefs::kCohorts);

        const std::optional<int> stored_int = dict.FindInt(key_hash);
        if (stored_int.has_value()) {
          auto stored_cohort =
              static_cast<content_settings::mojom::TpcdMetadataCohort>(
                  stored_int.value());

          if (content_settings::mojom::IsKnownEnumValue(stored_cohort)) {
            cohort = stored_cohort;
            remove_keys.erase(key_hash);
          }
        }
      }
    }

    if (!cohort.has_value()) {
      cohort = GenerateRand() <= ElectedDtrp(metadata_entry)
                   ? content_settings::mojom::TpcdMetadataCohort::
                         GRACE_PERIOD_FORCED_OFF
                   : content_settings::mojom::TpcdMetadataCohort::
                         GRACE_PERIOD_FORCED_ON;

      if (local_state_) {
        ScopedDictPrefUpdate update(local_state_, prefs::kCohorts);
        update->Set(key_hash, static_cast<int32_t>(cohort.value()));
      }
    }

    CHECK(cohort.has_value());
    rule_metadata.set_tpcd_metadata_cohort(cohort.value());

    grants.emplace_back(primary_pattern, secondary_pattern, std::move(value),
                        /*source=*/std::string(), /*incognito=*/false,
                        std::move(rule_metadata));
  }

  if (base::FeatureList::IsEnabled(
          net::features::kTpcdMetadataStagedRollback) &&
      local_state_) {
    ScopedDictPrefUpdate update(local_state_, prefs::kCohorts);
    for (const auto& key : remove_keys) {
      update->Remove(key);
    }
  }

  SetGrants(grants);
}

ContentSettingsForOneType Manager::GetGrants() const {

  if (!base::FeatureList::IsEnabled(net::features::kTpcdMetadataGrants)) {
    return ContentSettingsForOneType();
  }

  if (base::FeatureList::IsEnabled(
          content_settings::features::kHostIndexedMetadataGrants)) {
    base::AutoLock lock(grants_lock_);
    return GetContentSettingForOneType(
        absl::get<content_settings::HostIndexedContentSettings>(grants_));
  }

  base::AutoLock lock(grants_lock_);
  return absl::get<ContentSettingsForOneType>(grants_);
}

}  // namespace tpcd::metadata
