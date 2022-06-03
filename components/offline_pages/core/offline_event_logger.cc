// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_event_logger.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/offline_pages/core/offline_clock.h"

namespace offline_pages {

OfflineEventLogger::OfflineEventLogger()
    : activities_(0), is_logging_(false), client_(nullptr) {}

OfflineEventLogger::~OfflineEventLogger() {}

void OfflineEventLogger::Clear() {
  activities_.clear();
}

void OfflineEventLogger::SetIsLogging(bool is_logging) {
  is_logging_ = is_logging;
}

bool OfflineEventLogger::GetIsLogging() {
  return is_logging_;
}

void OfflineEventLogger::GetLogs(std::vector<std::string>* records) {
  DCHECK(records);
  records->insert(records->end(), activities_.begin(), activities_.end());
}

void OfflineEventLogger::RecordActivity(const std::string& activity) {
  DVLOG(1) << activity;
  if (!is_logging_ || activity.empty())
    return;

  base::Time::Exploded current_time;
  OfflineTimeNow().LocalExplode(&current_time);

  std::string date_string = base::StringPrintf(
      "%d %02d %02d %02d:%02d:%02d", current_time.year, current_time.month,
      current_time.day_of_month, current_time.hour, current_time.minute,
      current_time.second);

  if (client_)
    client_->CustomLog(activity);

  if (activities_.size() == kMaxLogCount)
    activities_.pop_back();

  activities_.push_front(date_string + ": " + activity);
}

void OfflineEventLogger::SetClient(Client* client) {
  DCHECK(client);
  SetIsLogging(true);
  client_ = client;
}

}  // namespace offline_pages
