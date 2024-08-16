// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_user_events/user_event_service_impl.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
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
    case UserEventSpecifics::kGaiaPasswordReuseEvent:
      return kMustHave;
    case UserEventSpecifics::kGaiaPasswordCapturedEvent:
    case UserEventSpecifics::kFlocIdComputedEvent:
      return kCannotHave;
    // The event types below are not recorded anymore, so are not handled here
    // (will fall through to the NOTREACHED() below).
    case UserEventSpecifics::kLanguageDetectionEvent:
    case UserEventSpecifics::kTranslationEvent:
    case UserEventSpecifics::EVENT_NOT_SET:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return kEitherOkay;
}

bool NavigationPresenceValid(UserEventSpecifics::EventCase event_case,
                             bool has_navigation_id) {
  NavigationPresence presence = GetNavigationPresence(event_case);
  return presence == kEitherOkay ||
         (presence == kMustHave && has_navigation_id) ||
         (presence == kCannotHave && !has_navigation_id);
}

// An equivalent to UserEventSpecifics::EventCase (from the proto) that's
// appropriate for recording in UMA. These values are persisted to logs. Entries
// should not be renumbered and numeric values should never be reused. Keep in
// sync with SyncUserEventType in
// tools/metrics/histograms/metadata/sync/enums.xml.
// LINT.IfChange(SyncUserEventType)
enum class EventTypeForUMA {
  kUnknown = 0,
  kTestEvent = 1,
  kGaiaPasswordReuseEvent = 2,
  kGaiaPasswordCapturedEvent = 3,
  kFlocIdComputedEvent = 4,
  kMaxValue = kFlocIdComputedEvent
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:SyncUserEventType)

EventTypeForUMA GetEventTypeForUMA(UserEventSpecifics::EventCase event_case) {
  switch (event_case) {
    case UserEventSpecifics::kTestEvent:
      return EventTypeForUMA::kTestEvent;
    case UserEventSpecifics::kGaiaPasswordReuseEvent:
      return EventTypeForUMA::kGaiaPasswordReuseEvent;
    case UserEventSpecifics::kGaiaPasswordCapturedEvent:
      return EventTypeForUMA::kGaiaPasswordCapturedEvent;
    case UserEventSpecifics::kFlocIdComputedEvent:
      return EventTypeForUMA::kFlocIdComputedEvent;
    // The event types below are not recorded anymore, so are not handled here
    // (will fall through to the NOTREACHED() below).
    case UserEventSpecifics::kLanguageDetectionEvent:
    case UserEventSpecifics::kTranslationEvent:
    case UserEventSpecifics::EVENT_NOT_SET:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return EventTypeForUMA::kUnknown;
}

}  // namespace

UserEventServiceImpl::UserEventServiceImpl(
    std::unique_ptr<UserEventSyncBridge> bridge)
    : bridge_(std::move(bridge)), session_id_(base::RandUint64()) {
  DCHECK(bridge_);
}

UserEventServiceImpl::~UserEventServiceImpl() = default;

void UserEventServiceImpl::Shutdown() {}

void UserEventServiceImpl::RecordUserEvent(
    std::unique_ptr<UserEventSpecifics> specifics) {
  if (!ShouldRecordEvent(*specifics)) {
    return;
  }

  DCHECK(!specifics->has_session_id());
  specifics->set_session_id(session_id_);

  base::UmaHistogramEnumeration("Sync.RecordedUserEventType",
                                GetEventTypeForUMA(specifics->event_case()));

  bridge_->RecordUserEvent(std::move(specifics));
}

void UserEventServiceImpl::RecordUserEvent(
    const UserEventSpecifics& specifics) {
  RecordUserEvent(std::make_unique<UserEventSpecifics>(specifics));
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
UserEventServiceImpl::GetControllerDelegate() {
  return bridge_->change_processor()->GetControllerDelegate();
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
