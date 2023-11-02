// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_ENTRY_H_
#define COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_ENTRY_H_

#include <string>

#include "base/time/time.h"
#include "url/gurl.h"

namespace sync_pb {
class SendTabToSelfSpecifics;
}

namespace send_tab_to_self {

constexpr base::TimeDelta kExpiryTime = base::Days(10);

class SendTabToSelfLocal;

// A tab that is being shared. The URL is a unique identifier for an entry, as
// such it should not be empty and is the only thing considered when comparing
// entries.
// The java version of this class: SendTabToSelfEntry.java
class SendTabToSelfEntry {
 public:
  // Creates a SendTabToSelf entry. |url| and |title| are the main fields of the
  // entry.
  // |now| is used to fill the |creation_time_us_| and all the update timestamp
  // fields.
  SendTabToSelfEntry(const std::string& guid,
                     const GURL& url,
                     const std::string& title,
                     base::Time shared_time,
                     const std::string& device_name,
                     const std::string& target_device_sync_cache_guid);

  SendTabToSelfEntry(const SendTabToSelfEntry&);
  SendTabToSelfEntry& operator=(const SendTabToSelfEntry&) = delete;

  ~SendTabToSelfEntry();

  // The unique random id for the entry.
  const std::string& GetGUID() const;
  // The URL of the page the user would like to send to themselves.
  const GURL& GetURL() const;
  // The title of the entry. Might be empty.
  const std::string& GetTitle() const;
  // The time that the tab was shared.
  base::Time GetSharedTime() const;
  // The name of the device that originated the sent tab.
  const std::string& GetDeviceName() const;
  // The cache guid of of the device that this tab is shared with.
  const std::string& GetTargetDeviceSyncCacheGuid() const;
  // The opened state of the entry.
  bool IsOpened() const;
  // Sets the opened state of the entry to true.
  void MarkOpened();

  // The state of this entry's notification: if it has been |dismissed|.
  void SetNotificationDismissed(bool notification_dismissed);
  bool GetNotificationDismissed() const;

  // Returns a protobuf encoding the content of this SendTabToSelfEntry for
  // local storage.
  SendTabToSelfLocal AsLocalProto() const;

  // Creates a SendTabToSelfEntry from the protobuf format.
  // If creation time is not set, it will be set to |now|.
  static std::unique_ptr<SendTabToSelfEntry> FromProto(
      const sync_pb::SendTabToSelfSpecifics& pb_entry,
      base::Time now);

  // Creates a SendTabToSelfEntry from the protobuf format.
  // If creation time is not set, it will be set to |now|.
  static std::unique_ptr<SendTabToSelfEntry> FromLocalProto(
      const SendTabToSelfLocal& pb_entry,
      base::Time now);

  // Returns if the Entry has expired based on the |current_time|.
  bool IsExpired(base::Time current_time) const;

  // Creates a SendTabToSelfEntry consisting of only the required fields.
  // This entry will have an expired SharedTime and therefor this function
  // should only be used for testing.
  static std::unique_ptr<SendTabToSelfEntry> FromRequiredFields(
      const std::string& guid,
      const GURL& url,
      const std::string& target_device_sync_cache_guid);

 private:
  std::string guid_;
  GURL url_;
  std::string title_;
  std::string device_name_;
  std::string target_device_sync_cache_guid_;
  base::Time shared_time_;
  bool notification_dismissed_;
  bool opened_;
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_ENTRY_H_
