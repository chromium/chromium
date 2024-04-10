// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tpcd/metadata/manager.h"

#include <utility>

#include "base/check.h"
#include "base/no_destructor.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_rules.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/host_indexed_content_settings.h"
#include "components/tpcd/metadata/common/manager_base.h"
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

  base::AutoLock lock(grants_lock_);
  if (base::FeatureList::IsEnabled(
          content_settings::features::kHostIndexedMetadataGrants)) {
    grants_ = content_settings::HostIndexedContentSettings();
  } else {
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

void Manager::SetGrants(const ContentSettingsForOneType& grants) {
  base::AutoLock lock(grants_lock_);

  if (absl::holds_alternative<content_settings::HostIndexedContentSettings>(
          grants_)) {
    auto indices = content_settings::HostIndexedContentSettings::Create(grants);
    if (indices.empty()) {
      grants_ = content_settings::HostIndexedContentSettings();
    } else {
      CHECK_EQ(indices.size(), 1u);
      grants_ = std::move(indices.front());
    }
  } else {
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
    // TODO(http://b/330759665): Get the cohort from the experiment
    // picker/prefs.
    rule_metadata.set_tpcd_metadata_cohort(
        content_settings::mojom::TpcdMetadataCohort::DEFAULT);

    grants.emplace_back(primary_pattern, secondary_pattern, std::move(value),
                        /*source=*/std::string(), /*incognito=*/false,
                        std::move(rule_metadata));
  }

  SetGrants(grants);
}

ContentSettingsForOneType Manager::GetGrants() const {
  base::AutoLock lock(grants_lock_);

  if (!base::FeatureList::IsEnabled(net::features::kTpcdMetadataGrants)) {
    return ContentSettingsForOneType();
  }

  if (absl::holds_alternative<content_settings::HostIndexedContentSettings>(
          grants_)) {
    return GetContentSettingForOneType(
        absl::get<content_settings::HostIndexedContentSettings>(grants_));
  }

  return absl::get<ContentSettingsForOneType>(grants_);
}

}  // namespace tpcd::metadata
