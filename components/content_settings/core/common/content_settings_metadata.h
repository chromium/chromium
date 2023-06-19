// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_METADATA_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_METADATA_H_

#include "base/time/time.h"
#include "components/content_settings/core/common/content_settings_constraints.h"

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

  bool operator==(const RuleMetaData& other) const;

  base::Time last_modified() const { return last_modified_; }
  void set_last_modified(base::Time last_modified) {
    last_modified_ = last_modified;
  }

  base::Time last_visited() const { return last_visited_; }
  void set_last_visited(base::Time visited) { last_visited_ = visited; }

  base::Time expiration() const { return expiration_; }

  SessionModel session_model() const { return session_model_; }
  void set_session_model(SessionModel session_model) {
    session_model_ = session_model;
  }

  base::TimeDelta lifetime() const { return lifetime_; }

  // Sets member variables based on `constraints`.
  void SetFromConstraints(const ContentSettingConstraints& constraints);

  // Sets the expiration and lifetime. The expiration may be zero if-and-only-if
  // the lifetime is zero; otherwise, both must be non-zero. The lifetime must
  // be nonnegative.
  void SetExpirationAndLifetime(base::Time expiration,
                                base::TimeDelta lifetime);

  // Computes the setting's lifetime, based on the lifetime and expiration that
  // were read from persistent storage. This is a helper to deal with missing
  // lifetime data during migration/rollout.
  static base::TimeDelta ComputeLifetime(base::TimeDelta lifetime,
                                         base::Time expiration);

 private:
  // mojo (de)serialization needs access to private details.
  friend struct mojo::
      StructTraits<content_settings::mojom::RuleMetaDataDataView, RuleMetaData>;

  // Last Modified data as specified by some UserModifiableProvider
  // implementations. May be zero.
  base::Time last_modified_;
  // Last visited data as specified by some UserModifiableProvider
  // implementations. Only non-zero when
  // ContentSettingsConstraint::track_last_visit_for_autoexpiration is enabled.
  base::Time last_visited_;
  // Expiration date if defined through a ContentSettingsConstraint. May be
  // zero.
  base::Time expiration_;
  // SessionModel as defined through a ContentSettingsConstraint.
  SessionModel session_model_ = SessionModel::Durable;
  // The lifetime of the setting. This may be zero iff `expiration_` is zero.
  base::TimeDelta lifetime_;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_METADATA_H_
