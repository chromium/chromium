// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_bookmarks/core/powers/power.h"

namespace power_bookmarks {

Power::Power(std::unique_ptr<PowerSpecifics> power_specifics) {
  CHECK(power_specifics);
  power_specifics_ = std::move(power_specifics);
}

Power::Power(const PowerBookmarkSpecifics& specifics) {
  guid_ = base::GUID::ParseLowercase(specifics.guid());
  url_ = GURL(specifics.url());
  power_type_ = specifics.power_type();

  // See:
  // https://source.chromium.org/chromium/chromium/src/+/main:base/time/time.h;l=49-60;drc=e8d37dfe21715cd84536a1412b778e1a5a39fb0c
  time_added_ = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(specifics.creation_time_usec()));
  time_modified_ = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(specifics.update_time_usec()));

  power_specifics_ = std::make_unique<PowerSpecifics>();
  power_specifics_->CopyFrom(specifics.power_specifics());
}

Power::~Power() = default;

void Power::ToPowerBookmarkSpecifics(PowerBookmarkSpecifics* specifics) {
  specifics->set_guid(guid_.AsLowercaseString());
  specifics->set_url(url_.spec());
  specifics->set_power_type(power_type_);

  // See:
  // https://source.chromium.org/chromium/chromium/src/+/main:base/time/time.h;l=49-60;drc=e8d37dfe21715cd84536a1412b778e1a5a39fb0c
  specifics->set_creation_time_usec(
      time_added_.ToDeltaSinceWindowsEpoch().InMicroseconds());
  specifics->set_update_time_usec(
      time_modified_.ToDeltaSinceWindowsEpoch().InMicroseconds());

  specifics->mutable_power_specifics()->CopyFrom(*power_specifics_.get());
}

}  // namespace power_bookmarks