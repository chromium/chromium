// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/screentime/history_bridge.h"

#import <ScreenTime/ScreenTime.h>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cocoa/screentime/history_deleter.h"
#include "chrome/browser/ui/cocoa/screentime/screentime_policy.h"

namespace screentime {

HistoryBridge::HistoryBridge(history::HistoryService* service,
                             std::unique_ptr<HistoryDeleter> deleter)
    : deleter_(std::move(deleter)) {
  history_service_observer_.Observe(service);
}
HistoryBridge::~HistoryBridge() = default;

void HistoryBridge::OnURLsDeleted(history::HistoryService* service,
                                  const history::DeletionInfo& deletion_info) {
  if (deletion_info.IsAllHistory()) {
    deleter_->DeleteAllHistory();
  } else if (deletion_info.time_range().IsValid()) {
    if (deletion_info.restrict_urls().has_value()) {
      // Awkward: the ScreenTime API has no way to express "delete history for
      // this URL within this time range", only "delete all history for this
      // URL" and "delete all history within this time range". Here, we err on
      // side of deleting the specific URLs for all time, rather than deleting
      // all URLs within the given time.
      for (const auto& url : *deletion_info.restrict_urls())
        deleter_->DeleteHistoryForURL(url);
    } else {
      deleter_->DeleteHistoryDuringInterval(
          std::make_pair(deletion_info.time_range().begin(),
                         deletion_info.time_range().end()));
    }
  } else {
    // If the time range isn't valid at all, this is a URL delete, which has no
    // time bounds.
    for (const auto& row : deletion_info.deleted_rows())
      deleter_->DeleteHistoryForURL(URLForReporting(row.url()));
  }
}

void HistoryBridge::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  DCHECK(history_service_observer_.IsObservingSource(history_service));
  history_service_observer_.Reset();
}

}  // namespace screentime
