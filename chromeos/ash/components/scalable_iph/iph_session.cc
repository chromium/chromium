// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/scalable_iph/iph_session.h"

#include "base/feature_list.h"
#include "components/feature_engagement/public/tracker.h"

namespace scalable_iph {

IphSession::IphSession(const base::Feature& feature,
                       feature_engagement::Tracker* tracker)
    : feature_(feature), tracker_(tracker) {
  CHECK(tracker_);
}

IphSession::~IphSession() {
  tracker_->Dismissed(feature_);
}

}  // namespace scalable_iph
