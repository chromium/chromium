// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/scalable_iph/iph_session.h"

#include "base/feature_list.h"
#include "components/feature_engagement/public/tracker.h"

namespace scalable_iph {

IphSession::IphSession(const base::Feature& feature,
                       feature_engagement::Tracker* tracker,
                       Delegate* delegate)
    : feature_(feature), tracker_(tracker), delegate_(delegate) {
  CHECK(tracker_);
  CHECK(delegate_);
}

IphSession::~IphSession() {
  tracker_->Dismissed(*feature_);
}

void IphSession::PerformAction(ActionType action_type,
                               const std::string& event_name) {
  if (!event_name.empty()) {
    tracker_->NotifyEvent(event_name);
  }
  delegate_->PerformActionForIphSession(action_type);
}

}  // namespace scalable_iph
