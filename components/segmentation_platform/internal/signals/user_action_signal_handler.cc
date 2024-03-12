// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/signals/user_action_signal_handler.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/metrics_hashes.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/internal/database/ukm_database.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/public/proto/types.pb.h"

namespace segmentation_platform {

UserActionSignalHandler::UserActionSignalHandler(
    const std::string& profile_id,
    SignalDatabase* signal_database,
    UkmDatabase* ukm_db)
    : profile_id_(profile_id),
      db_(signal_database),
      ukm_db_(ukm_db),
      metrics_enabled_(false) {
  action_callback_ = base::BindRepeating(&UserActionSignalHandler::OnUserAction,
                                         weak_ptr_factory_.GetWeakPtr());
}

UserActionSignalHandler::~UserActionSignalHandler() {
  DCHECK(observers_.empty());
  if (metrics_enabled_ && base::GetRecordActionTaskRunner())
    base::RemoveActionCallback(action_callback_);
}

void UserActionSignalHandler::EnableMetrics(bool enable_metrics) {
  if (metrics_enabled_ == enable_metrics || !base::GetRecordActionTaskRunner())
    return;

  metrics_enabled_ = enable_metrics;

  // As an added optimization, we unregister the callback when metrics is
  // disabled.
  if (metrics_enabled_) {
    base::AddActionCallback(action_callback_);
  } else {
    base::RemoveActionCallback(action_callback_);
  }
}

void UserActionSignalHandler::SetRelevantUserActions(
    std::set<uint64_t> user_actions) {
  user_actions_ = std::move(user_actions);
}

void UserActionSignalHandler::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void UserActionSignalHandler::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void UserActionSignalHandler::OnUserAction(const std::string& user_action,
                                           base::TimeTicks action_time) {
  DCHECK(metrics_enabled_);
  uint64_t user_action_hash = base::HashMetricName(user_action);
  auto iter = user_actions_.find(user_action_hash);
  if (iter == user_actions_.end())
    return;

  db_->WriteSample(
      proto::SignalType::USER_ACTION, user_action_hash, std::nullopt,
      base::BindOnce(&UserActionSignalHandler::OnSampleWritten,
                     weak_ptr_factory_.GetWeakPtr(), user_action, action_time));
  if (ukm_db_) {
    ukm_db_->AddUmaMetric(profile_id_,
                          UmaMetricEntry{.type = proto::SignalType::USER_ACTION,
                                         .name_hash = user_action_hash,
                                         .time = base::Time::Now(),
                                         .value = 0});
  }
}

void UserActionSignalHandler::OnSampleWritten(const std::string& user_action,
                                              base::TimeTicks action_time,
                                              bool success) {
  if (!success) {
    return;
  }

  for (Observer& ob : observers_) {
    ob.OnUserAction(user_action, action_time);
  }
}

}  // namespace segmentation_platform
