// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/send_tab_to_self_entry.h"

#include <memory>

#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/send_tab_to_self/proto/send_tab_to_self.pb.h"
#include "components/sync/protocol/send_tab_to_self_specifics.pb.h"

namespace send_tab_to_self {

namespace {

// Converts a time object to the format used in sync protobufs (ms since the
// Windows epoch).
int64_t TimeToProtoTime(const base::Time t) {
  return t.ToDeltaSinceWindowsEpoch().InMicroseconds();
}

// Converts a time field from sync protobufs to a time object.
base::Time ProtoTimeToTime(int64_t proto_t) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(proto_t));
}

}  // namespace

SendTabToSelfEntry::SendTabToSelfEntry(
    const std::string& guid,
    const GURL& url,
    const std::string& title,
    base::Time shared_time,
    base::Time original_navigation_time,
    const std::string& device_name,
    const std::string& target_device_sync_cache_guid)
    : guid_(guid),
      url_(url),
      title_(title),
      device_name_(device_name),
      target_device_sync_cache_guid_(target_device_sync_cache_guid),
      shared_time_(shared_time),
      original_navigation_time_(original_navigation_time),
      notification_dismissed_(false),
      opened_(false) {
  DCHECK(!guid_.empty());
  DCHECK(url_.is_valid());
  DCHECK(base::IsStringUTF8(guid_));
  DCHECK(base::IsStringUTF8(title_));
  DCHECK(base::IsStringUTF8(target_device_sync_cache_guid_));
  DCHECK(base::IsStringUTF8(device_name_));
}

SendTabToSelfEntry::~SendTabToSelfEntry() {}

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

base::Time SendTabToSelfEntry::GetOriginalNavigationTime() const {
  return original_navigation_time_;
}

const std::string& SendTabToSelfEntry::GetDeviceName() const {
  return device_name_;
}

const std::string& SendTabToSelfEntry::GetTargetDeviceSyncCacheGuid() const {
  return target_device_sync_cache_guid_;
}

bool SendTabToSelfEntry::IsOpened() const {
  return opened_;
}

void SendTabToSelfEntry::MarkOpened() {
  opened_ = true;
}

void SendTabToSelfEntry::SetNotificationDismissed(bool notification_dismissed) {
  notification_dismissed_ = notification_dismissed;
}

bool SendTabToSelfEntry::GetNotificationDismissed() const {
  return notification_dismissed_;
}

SendTabToSelfLocal SendTabToSelfEntry::AsLocalProto() const {
  SendTabToSelfLocal local_entry;
  auto* pb_entry = local_entry.mutable_specifics();

  pb_entry->set_guid(GetGUID());
  pb_entry->set_title(GetTitle());
  pb_entry->set_url(GetURL().spec());
  pb_entry->set_shared_time_usec(TimeToProtoTime(GetSharedTime()));
  pb_entry->set_navigation_time_usec(
      TimeToProtoTime(GetOriginalNavigationTime()));
  pb_entry->set_device_name(GetDeviceName());
  pb_entry->set_target_device_sync_cache_guid(GetTargetDeviceSyncCacheGuid());
  pb_entry->set_opened(IsOpened());
  pb_entry->set_notification_dismissed(GetNotificationDismissed());

  return local_entry;
}

std::unique_ptr<SendTabToSelfEntry> SendTabToSelfEntry::FromProto(
    const sync_pb::SendTabToSelfSpecifics& pb_entry,
    base::Time now) {
  std::string guid(pb_entry.guid());
  if (guid.empty()) {
    return nullptr;
  }

  GURL url(pb_entry.url());

  if (!url.is_valid()) {
    return nullptr;
  }

  base::Time shared_time = ProtoTimeToTime(pb_entry.shared_time_usec());
  if (shared_time > now) {
    shared_time = now;
  }

  base::Time navigation_time;
  if (pb_entry.has_navigation_time_usec()) {
    navigation_time = ProtoTimeToTime(pb_entry.navigation_time_usec());
  }

  // Protobuf parsing enforces utf8 encoding for all strings.
  auto entry = std::make_unique<SendTabToSelfEntry>(
      guid, url, pb_entry.title(), shared_time, navigation_time,
      pb_entry.device_name(), pb_entry.target_device_sync_cache_guid());

  if (pb_entry.opened()) {
    entry->MarkOpened();
  }
  if (pb_entry.notification_dismissed()) {
    entry->SetNotificationDismissed(true);
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
    const std::string& guid,
    const GURL& url,
    const std::string& target_device_sync_cache_guid) {
  if (guid.empty() || !url.is_valid()) {
    return nullptr;
  }
  return std::make_unique<SendTabToSelfEntry>(guid, url, "", base::Time(),
                                              base::Time(), "",
                                              target_device_sync_cache_guid);
}

}  // namespace send_tab_to_self
