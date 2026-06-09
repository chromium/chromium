// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_ENTRY_H_
#define COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_ENTRY_H_

#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "url/gurl.h"

namespace sync_pb {
class SendTabToSelfSpecifics;
}

namespace send_tab_to_self {

inline constexpr base::TimeDelta kExpiryTime = base::Days(10);

// Maximum size of the PageContext proto in bytes. Arbitrarily chosen as
// sensible threshold to avoid running into the per-entity size limit enforced
// by Sync.
inline constexpr size_t kMaxPageContextSizeBytes = 4096;  // 4 KB

// Represents the captured back/forward navigation history of a shared tab.
// This is used to reconstruct the navigation stack on the target device
// when the kSendTabToSelfPropagateNavigationHistory feature is enabled.
struct NavigationHistory {
  NavigationHistory();

  // Constructs a NavigationHistory from a full list of navigations.
  // The list is automatically trimmed to include at most
  // sessions::gMaxPersistNavigationCount entries both before and after
  // the current_navigation_index to ensure the resulting sync entity
  // stays within size limits.
  NavigationHistory(
      std::vector<sessions::SerializedNavigationEntry> navigations,
      int current_navigation_index);

  NavigationHistory(const NavigationHistory&);
  NavigationHistory& operator=(const NavigationHistory&);
  ~NavigationHistory();

  // The subset of serialized navigation entries from the tab's history.
  std::vector<sessions::SerializedNavigationEntry> navigations;

  // The index of the currently active navigation within the 'navigations'
  // vector. Will be nullopt if the history is empty or invalid.
  std::optional<int> current_navigation_index;
};

class SendTabToSelfLocal;
// A tab that is being shared. The URL is a unique identifier for an entry, as
// such it should not be empty and is the only thing considered when comparing
// entries.
// The java version of this class: SendTabToSelfEntry.java
class SendTabToSelfEntry {
 public:
  // Creates a SendTabToSelf entry. `url` and `title` are the main fields of the
  // entry. `url` must be valid as per IsValidUrl().
  SendTabToSelfEntry(std::string guid,
                     const GURL& url,
                     std::string title,
                     base::Time shared_time,
                     std::string device_name,
                     std::string target_device_sync_cache_guid,
                     const PageContext& page_context,
                     NavigationHistory navigation_history);

  SendTabToSelfEntry(const SendTabToSelfEntry&);

  SendTabToSelfEntry& operator=(const SendTabToSelfEntry&) = default;

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
  // Sets the opened state of the entry to true and records the opened time.
  void MarkOpened(base::Time opened_time);
  // Time when this entry was opened on the target device, or a null time if
  // it hasn't been opened.
  base::Time GetOpenedTime() const;

  // Time when this entry was first received by the target device's bridge.
  void MarkReceived(base::Time received_time);
  bool IsReceived() const;
  base::Time GetReceivedTime() const;

  // The state of this entry's notification: if it has been `dismissed`.
  void SetNotificationDismissed(bool notification_dismissed);
  bool GetNotificationDismissed() const;

  // Returns the page context.
  const PageContext& GetPageContext() const;

  // Returns the navigation history.
  const NavigationHistory& GetNavigationHistory() const;

  // Returns a protobuf encoding the content of this SendTabToSelfEntry for
  // local storage.
  SendTabToSelfLocal AsLocalProto() const;

  // Returns true if the URL is valid and has a supported scheme (HTTP or
  // HTTPS).
  static bool IsValidUrl(const GURL& url);

  // Creates a SendTabToSelfEntry from the protobuf format.
  // If creation time is not set, it will be set to `now`.
  static std::unique_ptr<SendTabToSelfEntry> FromProto(
      const sync_pb::SendTabToSelfSpecifics& pb_entry,
      base::Time now);

  // Creates a SendTabToSelfEntry from the protobuf format.
  // If creation time is not set, it will be set to `now`.
  static std::unique_ptr<SendTabToSelfEntry> FromLocalProto(
      const SendTabToSelfLocal& pb_entry,
      base::Time now);

  // Returns if the Entry has expired based on the `current_time`.
  bool IsExpired(base::Time current_time) const;

  // Creates a SendTabToSelfEntry consisting of only the required fields.
  // This entry will have an expired SharedTime and therefor this function
  // should only be used for testing.
  static std::unique_ptr<SendTabToSelfEntry> FromRequiredFields(
      std::string guid,
      const GURL& url,
      std::string target_device_sync_cache_guid);

 private:
  std::string guid_;
  GURL url_;
  std::string title_;
  std::string device_name_;
  std::string target_device_sync_cache_guid_;
  base::Time shared_time_;
  bool notification_dismissed_;
  PageContext page_context_;
  NavigationHistory navigation_history_;
  base::Time received_time_;
  base::Time opened_time_;
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_ENTRY_H_
