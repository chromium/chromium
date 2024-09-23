// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_CONSTRAINTS_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_CONSTRAINTS_H_

#include "base/time/time.h"
#include "components/content_settings/core/common/content_settings_enums.mojom.h"

namespace content_settings {
// Constraints to be applied when setting a content setting.
class ContentSettingConstraints {
 public:
  // Creates a default set of constraints. The constraints do not expire, use
  // the Durable session model, and do not track the last visit for
  // autoexpiration.
  ContentSettingConstraints();

  // Creates a default set of constraints, using `now` as the "created_at" time.
  explicit ContentSettingConstraints(base::Time now);

  ContentSettingConstraints(ContentSettingConstraints&& other);
  ContentSettingConstraints(const ContentSettingConstraints& other);
  ContentSettingConstraints& operator=(ContentSettingConstraints&& other);
  ContentSettingConstraints& operator=(const ContentSettingConstraints& other);

  ~ContentSettingConstraints();

  bool operator==(const ContentSettingConstraints& other) const;
  bool operator!=(const ContentSettingConstraints& other) const;

  base::Time expiration() const {
    if (lifetime_.is_zero()) {
      return base::Time();
    }
    return created_at_ + lifetime_;
  }

  base::TimeDelta lifetime() const { return lifetime_; }

  // Sets the lifetime. The lifetime must be nonnegative.
  void set_lifetime(base::TimeDelta lifetime) {
    CHECK_GE(lifetime, base::TimeDelta());
    lifetime_ = lifetime;
  }

  mojom::SessionModel session_model() const { return session_model_; }
  void set_session_model(mojom::SessionModel model) { session_model_ = model; }

  bool track_last_visit_for_autoexpiration() const {
    return track_last_visit_for_autoexpiration_;
  }
  void set_track_last_visit_for_autoexpiration(bool track) {
    track_last_visit_for_autoexpiration_ = track;
  }

  bool decided_by_related_website_sets() const {
    return decided_by_related_website_sets_;
  }
  void set_decided_by_related_website_sets(
      bool granted_by_related_website_sets) {
    decided_by_related_website_sets_ = granted_by_related_website_sets;
  }

 private:
  // Tracks the base::Time that this instance was constructed. Copies and moves
  // reuse this time.
  base::Time created_at_;

  // Specification of the lifetime of the setting created with these
  // constraints. This controls when the setting expires.
  //
  // If the lifetime is zero, then the setting does not expire.
  //
  // TODO(crbug.com/40270137): created_at_ and lifetime_ need to be
  // persisted (likely in/by content_settings::RuleMetaData) and recreated in
  // order be useful. Otherwise, everything still operates in terms of
  // expirations.
  base::TimeDelta lifetime_;

  // Used to specify the lifetime model that should be used.
  mojom::SessionModel session_model_ = mojom::SessionModel::DURABLE;
  // Set to true to keep track of the last visit to the origin of this
  // permission.
  // This is used for the Safety check permission module and unrelated to the
  // "lifetime" keyword above.
  bool track_last_visit_for_autoexpiration_ = false;

  // Set to true if the storage access was decided by a Related Website Set.
  bool decided_by_related_website_sets_ = false;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_CONSTRAINTS_H_
