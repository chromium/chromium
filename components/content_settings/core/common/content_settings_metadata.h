// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_METADATA_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_METADATA_H_

#include "base/time/time.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_enums.mojom.h"

namespace base {
class Clock;
}

namespace mojo {
template <typename DataViewType, typename T>
struct StructTraits;
}  // namespace mojo
namespace content_settings::mojom {
class RuleMetaDataDataView;
}  // namespace content_settings::mojom

namespace content_settings {

// Holds metadata for a ContentSetting rule.
class RuleMetaData {
 public:
  RuleMetaData();
  RuleMetaData(const RuleMetaData& other);
  RuleMetaData(RuleMetaData&& other);
  RuleMetaData& operator=(const RuleMetaData& other);
  RuleMetaData& operator=(RuleMetaData&& other);

  bool operator==(const RuleMetaData& other) const;

  base::Time last_modified() const { return last_modified_; }
  void set_last_modified(base::Time last_modified) {
    last_modified_ = last_modified;
  }

  base::Time last_used() const { return last_used_; }
  void set_last_used(base::Time last_used) { last_used_ = last_used; }

  base::Time last_visited() const { return last_visited_; }
  void set_last_visited(base::Time visited) { last_visited_ = visited; }

  base::Time expiration() const { return expiration_; }

  mojom::SessionModel session_model() const { return session_model_; }
  void set_session_model(mojom::SessionModel session_model) {
    session_model_ = session_model;
  }

  mojom::TpcdMetadataRuleSource tpcd_metadata_rule_source() const {
    return tpcd_metadata_rule_source_;
  }
  void set_tpcd_metadata_rule_source(
      mojom::TpcdMetadataRuleSource const rule_source) {
    tpcd_metadata_rule_source_ = rule_source;
  }

  mojom::TpcdMetadataCohort tpcd_metadata_cohort() const {
    return tpcd_metadata_cohort_;
  }
  void set_tpcd_metadata_cohort(const mojom::TpcdMetadataCohort cohort) {
    tpcd_metadata_cohort_ = cohort;
  }

  uint32_t tpcd_metadata_elected_dtrp() const {
    return tpcd_metadata_elected_dtrp_;
  }
  void set_tpcd_metadata_elected_dtrp(uint32_t elected_dtrp) {
    tpcd_metadata_elected_dtrp_ = elected_dtrp;
  }

  base::TimeDelta lifetime() const { return lifetime_; }

  // Sets member variables based on `constraints`.
  void SetFromConstraints(const ContentSettingConstraints& constraints);

  // Sets the expiration and lifetime. The expiration may be zero if-and-only-if
  // the lifetime is zero; otherwise, both must be non-zero. The lifetime must
  // be nonnegative.
  void SetExpirationAndLifetime(base::Time expiration,
                                base::TimeDelta lifetime);

  // Returns whether the Rule is expired. Expiration is handled by
  // HostContentSettingsMap automatically, clients do not have to check this
  // attribute manually.
  bool IsExpired(const base::Clock* clock) const;

  // Computes the setting's lifetime, based on the lifetime and expiration that
  // were read from persistent storage.
  static base::TimeDelta ComputeLifetime(base::TimeDelta lifetime,
                                         base::Time expiration);

  bool decided_by_related_website_sets() const {
    return decided_by_related_website_sets_;
  }
  void set_decided_by_related_website_sets(
      bool decided_by_related_website_sets) {
    decided_by_related_website_sets_ = decided_by_related_website_sets;
  }

 private:
  // mojo (de)serialization needs access to private details.
  friend struct mojo::
      StructTraits<content_settings::mojom::RuleMetaDataDataView, RuleMetaData>;

  // Last Modified data as specified by some UserModifiableProvider
  // implementations. May be zero.
  base::Time last_modified_;
  // Date when a permission-gate feature was used the last time. Verifying
  // and/or changing the value of `ContentSetting` is not considered as `used`.
  base::Time last_used_;
  // Last visited data as specified by some UserModifiableProvider
  // implementations. Only non-zero when
  // ContentSettingsConstraint::track_last_visit_for_autoexpiration is enabled.
  base::Time last_visited_;
  // Expiration date if defined through a ContentSettingsConstraint. May be
  // zero.
  base::Time expiration_;
  // SessionModel as defined through a ContentSettingsConstraint.
  mojom::SessionModel session_model_ = mojom::SessionModel::DURABLE;
  // The lifetime of the setting. This may be zero iff `expiration_` is zero.
  base::TimeDelta lifetime_;
  // TPCD Metadata Source (go/measure3pcddtdeployment).
  // TODO(http://b/324406007): The impl is currently specific to the TPCD
  // Metadata Source and is expected to be cleaned up with the mitigation
  // cleanup.
  mojom::TpcdMetadataRuleSource tpcd_metadata_rule_source_ =
      mojom::TpcdMetadataRuleSource::SOURCE_UNSPECIFIED;
  mojom::TpcdMetadataCohort tpcd_metadata_cohort_ =
      mojom::TpcdMetadataCohort::DEFAULT;
  uint32_t tpcd_metadata_elected_dtrp_ = 0u;

  // Set to true if the storage access was decided by a Related Website Set.
  bool decided_by_related_website_sets_ = false;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_METADATA_H_
