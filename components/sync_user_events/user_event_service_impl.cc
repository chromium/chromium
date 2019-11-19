// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_user_events/user_event_service_impl.h"

#include <utility>

#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "components/sync_user_events/user_event_sync_bridge.h"

using sync_pb::UserEventSpecifics;

namespace syncer {

namespace {

enum NavigationPresence {
  kMustHave,
  kCannotHave,
  kEitherOkay,
};

NavigationPresence GetNavigationPresence(
    UserEventSpecifics::EventCase event_case) {
  switch (event_case) {
    case UserEventSpecifics::kTestEvent:
      return kEitherOkay;
    case UserEventSpecifics::kFieldTrialEvent:
      return kCannotHave;
    case UserEventSpecifics::kLanguageDetectionEvent:
      return kMustHave;
    case UserEventSpecifics::kTranslationEvent:
      return kMustHave;
    case UserEventSpecifics::kUserConsent:
      return kCannotHave;
    case UserEventSpecifics::kGaiaPasswordReuseEvent:
      return kMustHave;
    case UserEventSpecifics::kGaiaPasswordCapturedEvent:
      return kCannotHave;
    case UserEventSpecifics::EVENT_NOT_SET:
      break;
  }
  NOTREACHED();
  return kEitherOkay;
}

bool NavigationPresenceValid(UserEventSpecifics::EventCase event_case,
                             bool has_navigation_id) {
  NavigationPresence presence = GetNavigationPresence(event_case);
  return presence == kEitherOkay ||
         (presence == kMustHave && has_navigation_id) ||
         (presence == kCannotHave && !has_navigation_id);
}

}  // namespace

UserEventServiceImpl::UserEventServiceImpl(
    std::unique_ptr<UserEventSyncBridge> bridge)
    : bridge_(std::move(bridge)), session_id_(base::RandUint64()) {
  DCHECK(bridge_);
}

UserEventServiceImpl::~UserEventServiceImpl() {}

void UserEventServiceImpl::Shutdown() {}

void UserEventServiceImpl::RecordUserEvent(
    std::unique_ptr<UserEventSpecifics> specifics) {
  if (ShouldRecordEvent(*specifics)) {
    DCHECK(!specifics->has_session_id());
    specifics->set_session_id(session_id_);
    bridge_->RecordUserEvent(std::move(specifics));
  }
}

void UserEventServiceImpl::RecordUserEvent(
    const UserEventSpecifics& specifics) {
  RecordUserEvent(std::make_unique<UserEventSpecifics>(specifics));
}

ModelTypeSyncBridge* UserEventServiceImpl::GetSyncBridge() {
  return bridge_.get();
}

bool UserEventServiceImpl::ShouldRecordEvent(
    const UserEventSpecifics& specifics) {
  if (specifics.event_case() == UserEventSpecifics::EVENT_NOT_SET) {
    return false;
  }

  if (!NavigationPresenceValid(specifics.event_case(),
                               specifics.has_navigation_id())) {
    return false;
  }

  return true;
}

}  // namespace syncer
