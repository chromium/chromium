// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_bookmarks/common/power.h"

namespace power_bookmarks {

Power::Power(std::unique_ptr<sync_pb::PowerEntity> power_entity) {
  CHECK(power_entity);
  power_entity_ = std::move(power_entity);
}

Power::Power(const sync_pb::PowerBookmarkSpecifics& specifics) {
  guid_ = base::Uuid::ParseLowercase(specifics.guid());
  url_ = GURL(specifics.url());
  power_type_ = specifics.power_type();

  // See:
  // https://source.chromium.org/chromium/chromium/src/+/main:base/time/time.h;l=49-60;drc=e8d37dfe21715cd84536a1412b778e1a5a39fb0c
  time_added_ = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(specifics.creation_time_usec()));
  time_modified_ = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(specifics.update_time_usec()));

  power_entity_ = std::make_unique<sync_pb::PowerEntity>();
  power_entity_->CopyFrom(specifics.power_entity());
}

Power::~Power() = default;

void Power::ToPowerBookmarkSpecifics(
    sync_pb::PowerBookmarkSpecifics* specifics) const {
  specifics->set_guid(guid_.AsLowercaseString());
  specifics->set_url(url_.spec());
  specifics->set_power_type(power_type_);

  // See:
  // https://source.chromium.org/chromium/chromium/src/+/main:base/time/time.h;l=49-60;drc=e8d37dfe21715cd84536a1412b778e1a5a39fb0c
  specifics->set_creation_time_usec(
      time_added_.ToDeltaSinceWindowsEpoch().InMicroseconds());
  specifics->set_update_time_usec(
      time_modified_.ToDeltaSinceWindowsEpoch().InMicroseconds());

  specifics->mutable_power_entity()->CopyFrom(*power_entity_.get());
}

void Power::Merge(const Power& other) {
  // Assuming guid, url and power type are the same.
  DCHECK(guid_ == other.guid_);
  DCHECK(url_ == other.url_);
  DCHECK(power_type_ == other.power_type_);
  // Take the power entity of the one with a more recent modified time.
  if (time_modified_ < other.time_modified_) {
    time_modified_ = other.time_modified_;
    // TODO(crbug.com/40245833): Powers should be able to customize the merge
    // logic.
    power_entity_->CopyFrom(*other.power_entity_);
  }
}

std::unique_ptr<Power> Power::Clone() const {
  sync_pb::PowerBookmarkSpecifics power_specifics;
  ToPowerBookmarkSpecifics(&power_specifics);
  return std::make_unique<Power>(power_specifics);
}

}  // namespace power_bookmarks
