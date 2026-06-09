// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/send_tab_to_self_entry.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/send_tab_to_self/proto/send_tab_to_self.pb.h"
#include "components/send_tab_to_self/proto_conversions.h"
#include "components/sessions/core/session_constants.h"
#include "components/sync/protocol/send_tab_to_self_specifics.pb.h"
#include "components/sync_sessions/synced_session.h"

namespace send_tab_to_self {

NavigationHistory::NavigationHistory() = default;

NavigationHistory::NavigationHistory(
    std::vector<sessions::SerializedNavigationEntry> navigations_in,
    int current_navigation_index_in) {
  if (!base::FeatureList::IsEnabled(kSendTabToSelfPropagateNavigationHistory) ||
      navigations_in.empty()) {
    return;
  }
  CHECK_GE(current_navigation_index_in, 0);
  CHECK_LT(current_navigation_index_in,
           static_cast<int>(navigations_in.size()));

  // Trims the navigation history to include at most
  // `sessions::gMaxPersistNavigationCount` entries before and after the current
  // navigation.
  const int index = current_navigation_index_in;
  const int min_index =
      std::max(0, index - sessions::gMaxPersistNavigationCount);
  const int max_index = std::min(index + sessions::gMaxPersistNavigationCount,
                                 static_cast<int>(navigations_in.size()) - 1);

  navigations.assign(
      std::make_move_iterator(navigations_in.begin() + min_index),
      std::make_move_iterator(navigations_in.begin() + max_index + 1));
  current_navigation_index = index - min_index;
}

NavigationHistory::NavigationHistory(const NavigationHistory&) = default;

NavigationHistory& NavigationHistory::operator=(const NavigationHistory&) =
    default;

NavigationHistory::~NavigationHistory() = default;

namespace {

// Converts a time object to the format used in sync protobufs (microseconds
// since the Windows epoch).
int64_t TimeToProtoTime(const base::Time t) {
  return t.ToDeltaSinceWindowsEpoch().InMicroseconds();
}

// Converts a time field from sync protobufs to a time object.
base::Time ProtoTimeToTime(int64_t proto_t) {
  return base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(proto_t));
}

}  // namespace

SendTabToSelfEntry::SendTabToSelfEntry(
    std::string guid,
    const GURL& url,
    std::string title,
    base::Time shared_time,
    std::string device_name,
    std::string target_device_sync_cache_guid,
    const PageContext& page_context,
    NavigationHistory navigation_history)
    : guid_(std::move(guid)),
      url_(url),
      title_(std::move(title)),
      device_name_(std::move(device_name)),
      target_device_sync_cache_guid_(std::move(target_device_sync_cache_guid)),
      shared_time_(shared_time),
      notification_dismissed_(false),
      page_context_(page_context),
      navigation_history_(std::move(navigation_history)) {
  DCHECK(!guid_.empty());
  DCHECK(IsValidUrl(url_));
}

SendTabToSelfEntry::~SendTabToSelfEntry() = default;

SendTabToSelfEntry::SendTabToSelfEntry(const SendTabToSelfEntry&) = default;

const std::string& SendTabToSelfEntry::GetGUID() const {
  return guid_;
}

const GURL& SendTabToSelfEntry::GetURL() const {
  return url_;
}

const std::string& SendTabToSelfEntry::GetTitle() const {
  return title_;
}

base::Time SendTabToSelfEntry::GetSharedTime() const {
  return shared_time_;
}

const std::string& SendTabToSelfEntry::GetDeviceName() const {
  return device_name_;
}

const std::string& SendTabToSelfEntry::GetTargetDeviceSyncCacheGuid() const {
  return target_device_sync_cache_guid_;
}

bool SendTabToSelfEntry::IsOpened() const {
  return !opened_time_.is_null();
}

void SendTabToSelfEntry::MarkOpened(base::Time opened_time) {
  if (opened_time_.is_null()) {
    opened_time_ = opened_time;
  }
}

base::Time SendTabToSelfEntry::GetOpenedTime() const {
  return opened_time_;
}

base::Time SendTabToSelfEntry::GetReceivedTime() const {
  return received_time_;
}

void SendTabToSelfEntry::MarkReceived(base::Time received_time) {
  if (received_time_.is_null()) {
    received_time_ = received_time;
  }
}

bool SendTabToSelfEntry::IsReceived() const {
  return !received_time_.is_null();
}

void SendTabToSelfEntry::SetNotificationDismissed(bool notification_dismissed) {
  notification_dismissed_ = notification_dismissed;
}

bool SendTabToSelfEntry::GetNotificationDismissed() const {
  return notification_dismissed_;
}

const PageContext& SendTabToSelfEntry::GetPageContext() const {
  return page_context_;
}

const NavigationHistory& SendTabToSelfEntry::GetNavigationHistory() const {
  return navigation_history_;
}

SendTabToSelfLocal SendTabToSelfEntry::AsLocalProto() const {
  SendTabToSelfLocal local_entry;
  sync_pb::SendTabToSelfSpecifics* pb_entry = local_entry.mutable_specifics();

  pb_entry->set_guid(GetGUID());
  pb_entry->set_title(GetTitle());
  pb_entry->set_url(GetURL().spec());
  pb_entry->set_shared_time_usec(TimeToProtoTime(GetSharedTime()));
  pb_entry->set_device_name(GetDeviceName());
  pb_entry->set_target_device_sync_cache_guid(GetTargetDeviceSyncCacheGuid());
  pb_entry->set_opened(IsOpened());
  pb_entry->set_notification_dismissed(GetNotificationDismissed());

  if (base::FeatureList::IsEnabled(kSendTabToSelfPropagateNavigationHistory)) {
    pb_entry->mutable_navigation()->Reserve(
        navigation_history_.navigations.size());
    for (const sessions::SerializedNavigationEntry& navigation :
         navigation_history_.navigations) {
      *pb_entry->add_navigation() =
          sync_sessions::SessionNavigationToSyncData(navigation);
    }

    if (navigation_history_.current_navigation_index.has_value()) {
      pb_entry->set_current_navigation_index(
          *navigation_history_.current_navigation_index);
    }
  }

  if (IsReceived()) {
    pb_entry->set_received_time_windows_epoch_micros(
        TimeToProtoTime(GetReceivedTime()));
  }
  if (IsOpened()) {
    pb_entry->set_opened_time_windows_epoch_micros(
        TimeToProtoTime(GetOpenedTime()));
  }

  sync_pb::PageContext pb_page_context = PageContextToProto(page_context_);
  if (const size_t size = pb_page_context.ByteSizeLong();
      size > 0 && size <= kMaxPageContextSizeBytes) {
    *pb_entry->mutable_page_context() = std::move(pb_page_context);
  }

  return local_entry;
}

// static
bool SendTabToSelfEntry::IsValidUrl(const GURL& url) {
  return url.is_valid() && url.SchemeIsHTTPOrHTTPS();
}

std::unique_ptr<SendTabToSelfEntry> SendTabToSelfEntry::FromProto(
    const sync_pb::SendTabToSelfSpecifics& pb_entry,
    base::Time now) {
  std::string guid(pb_entry.guid());
  if (guid.empty()) {
    return nullptr;
  }

  GURL url(pb_entry.url());

  if (!IsValidUrl(url)) {
    return nullptr;
  }

  base::Time shared_time = ProtoTimeToTime(pb_entry.shared_time_usec());
  if (shared_time > now) {
    shared_time = now;
  }

  // Protobuf parsing enforces utf8 encoding for all strings.
  NavigationHistory navigation_history;
  if (base::FeatureList::IsEnabled(kSendTabToSelfPropagateNavigationHistory) &&
      pb_entry.navigation_size() > 0 &&
      pb_entry.has_current_navigation_index()) {
    const int current_navigation_index = pb_entry.current_navigation_index();
    if (current_navigation_index >= 0 &&
        current_navigation_index < pb_entry.navigation_size()) {
      std::vector<sessions::SerializedNavigationEntry> navigations;
      navigations.reserve(pb_entry.navigation_size());
      for (int i = 0; i < pb_entry.navigation_size(); ++i) {
        navigations.push_back(sync_sessions::SessionNavigationFromSyncData(
            i, pb_entry.navigation(i)));
      }
      navigation_history =
          NavigationHistory(std::move(navigations), current_navigation_index);
    }
  }

  auto entry = std::make_unique<SendTabToSelfEntry>(
      std::move(guid), url, pb_entry.title(), shared_time,
      pb_entry.device_name(), pb_entry.target_device_sync_cache_guid(),
      PageContextFromProto(pb_entry.page_context()),
      std::move(navigation_history));

  if (pb_entry.has_opened_time_windows_epoch_micros()) {
    // New clients populate the opened_time field.
    entry->MarkOpened(
        ProtoTimeToTime(pb_entry.opened_time_windows_epoch_micros()));
  } else if (pb_entry.opened()) {
    // Fallback for old clients that only set the opened bool without a
    // timestamp. Use the shared_time as a best-effort approximation.
    entry->MarkOpened(shared_time);
  }
  if (pb_entry.notification_dismissed()) {
    entry->SetNotificationDismissed(true);
  }

  if (pb_entry.has_received_time_windows_epoch_micros()) {
    entry->MarkReceived(
        ProtoTimeToTime(pb_entry.received_time_windows_epoch_micros()));
  }

  return entry;
}

std::unique_ptr<SendTabToSelfEntry> SendTabToSelfEntry::FromLocalProto(
    const SendTabToSelfLocal& local_entry,
    base::Time now) {
  // No fields are currently read from the local proto.
  return FromProto(local_entry.specifics(), now);
}

bool SendTabToSelfEntry::IsExpired(base::Time current_time) const {
  return (current_time.ToDeltaSinceWindowsEpoch() -
              GetSharedTime().ToDeltaSinceWindowsEpoch() >=
          kExpiryTime);
}

std::unique_ptr<SendTabToSelfEntry> SendTabToSelfEntry::FromRequiredFields(
    std::string guid,
    const GURL& url,
    std::string target_device_sync_cache_guid) {
  if (guid.empty() || !IsValidUrl(url)) {
    return nullptr;
  }
  return std::make_unique<SendTabToSelfEntry>(
      std::move(guid), url, "", base::Time(), "",
      std::move(target_device_sync_cache_guid), PageContext{},
      NavigationHistory{});
}

}  // namespace send_tab_to_self
