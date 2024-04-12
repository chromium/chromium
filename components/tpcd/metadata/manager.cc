// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tpcd/metadata/manager.h"

#include <random>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_enums.mojom-shared.h"
#include "components/content_settings/core/common/content_settings_rules.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/host_indexed_content_settings.h"
#include "components/tpcd/metadata/metadata.pb.h"
#include "components/tpcd/metadata/parser.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace tpcd::metadata {
// static
Manager* Manager::GetInstance(Parser* parser, GrantsSyncCallback callback) {
  static base::NoDestructor<Manager> instance(parser, std::move(callback));
  return instance.get();
}

Manager::Manager(Parser* parser, GrantsSyncCallback callback)
    : parser_(parser), grants_sync_callback_(std::move(callback)) {
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

    if (!Parser::IsDtrpEligible(
            Parser::ToRuleSource(metadata_entry.source())) ||
        !base::FeatureList::IsEnabled(
            net::features::kTpcdMetadataStagedRollback)) {
      rule_metadata.set_tpcd_metadata_cohort(
          content_settings::mojom::TpcdMetadataCohort::DEFAULT);
    } else {
      uint32_t elected_dtrp = metadata_entry.has_dtrp_override()
                                  ? metadata_entry.dtrp_override()
                                  : metadata_entry.dtrp();

      auto cohort = GenerateRand() <= elected_dtrp
                        ? content_settings::mojom::TpcdMetadataCohort::
                              GRACE_PERIOD_FORCED_OFF
                        : content_settings::mojom::TpcdMetadataCohort::
                              GRACE_PERIOD_FORCED_ON;

      rule_metadata.set_tpcd_metadata_cohort(cohort);
    }

    grants.emplace_back(primary_pattern, secondary_pattern, std::move(value),
                        /*source=*/std::string(), /*incognito=*/false,
                        std::move(rule_metadata));
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
