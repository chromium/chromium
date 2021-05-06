// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/signals/user_action_signal_handler.h"

#include "base/metrics/metrics_hashes.h"
#include "components/segmentation_platform/internal/database/user_action_database.h"

namespace segmentation_platform {

UserActionSignalHandler::UserActionSignalHandler(
    UserActionDatabase* user_action_database)
    : db_(user_action_database), metrics_enabled_(false) {
  action_callback_ = base::BindRepeating(&UserActionSignalHandler::OnUserAction,
                                         weak_ptr_factory_.GetWeakPtr());
}

UserActionSignalHandler::~UserActionSignalHandler() {
  base::RemoveActionCallback(action_callback_);
}

void UserActionSignalHandler::EnableMetrics(bool enable_metrics) {
  if (metrics_enabled_ == enable_metrics)
    return;

  metrics_enabled_ = enable_metrics;
  if (metrics_enabled_)
    base::AddActionCallback(action_callback_);
  else
    base::RemoveActionCallback(action_callback_);
}

void UserActionSignalHandler::SetRelevantUserActions(
    std::set<uint64_t> user_actions) {
  user_actions_ = std::move(user_actions);
}

void UserActionSignalHandler::OnUserAction(const std::string& user_action,
                                           base::TimeTicks action_time) {
  DCHECK(metrics_enabled_);
  uint64_t user_action_hash = base::HashMetricName(user_action);
  auto iter = user_actions_.find(user_action_hash);
  if (iter == user_actions_.end())
    return;

  db_->WriteUserAction(user_action_hash, action_time);
}

}  // namespace segmentation_platform
