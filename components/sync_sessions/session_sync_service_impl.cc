// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/session_sync_service_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync_sessions/session_sync_bridge.h"
#include "components/sync_sessions/sync_sessions_client.h"

namespace sync_sessions {

SessionSyncServiceImpl::SessionSyncServiceImpl(
    version_info::Channel channel,
    std::unique_ptr<SyncSessionsClient> sessions_client)
    : sessions_client_(std::move(sessions_client)) {
  DCHECK(sessions_client_);

  bridge_ = std::make_unique<sync_sessions::SessionSyncBridge>(
      base::BindRepeating(&SessionSyncServiceImpl::NotifyForeignSessionUpdated,
                          base::Unretained(this)),
      sessions_client_.get(),
      std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
          syncer::SESSIONS,
          base::BindRepeating(&syncer::ReportUnrecoverableError, channel)));
}

SessionSyncServiceImpl::~SessionSyncServiceImpl() = default;

syncer::GlobalIdMapper* SessionSyncServiceImpl::GetGlobalIdMapper() const {
  return bridge_->GetGlobalIdMapper();
}

OpenTabsUIDelegate* SessionSyncServiceImpl::GetOpenTabsUIDelegate() {
  return bridge_->GetOpenTabsUIDelegate();
}

base::CallbackListSubscription
SessionSyncServiceImpl::SubscribeToForeignSessionsChanged(
    const base::RepeatingClosure& cb) {
  return foreign_sessions_changed_closure_list_.Add(cb);
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
SessionSyncServiceImpl::GetControllerDelegate() {
  return bridge_->change_processor()->GetControllerDelegate();
}

void SessionSyncServiceImpl::NotifyForeignSessionUpdated() {
  foreign_sessions_changed_closure_list_.Notify();
}

}  // namespace sync_sessions
