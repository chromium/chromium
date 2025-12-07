// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_SYNCED_SESSION_H_
#define COMPONENTS_SYNC_SESSIONS_SYNCED_SESSION_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/time/time.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "components/sync/protocol/session_specifics.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_device_info/device_info.h"

namespace sync_sessions {

// Construct a SerializedNavigationEntry for a particular index from a sync
// protocol buffer.  Note that the sync protocol buffer doesn't contain all
// SerializedNavigationEntry fields.  Also, the timestamp of the returned
// SerializedNavigationEntry is nulled out, as we assume that the protocol
// buffer is from a foreign session.
sessions::SerializedNavigationEntry SessionNavigationFromSyncData(
    int index,
    const sync_pb::TabNavigation& sync_data);

// Convert |navigation| into its sync protocol buffer equivalent. Note that the
// protocol buffer doesn't contain all SerializedNavigationEntry fields.
sync_pb::TabNavigation SessionNavigationToSyncData(
    const sessions::SerializedNavigationEntry& navigation);

// Set all the fields of |*tab| object from the given sync data and timestamp.
// Uses SerializedNavigationEntry::FromSyncData() to fill |navigations|. Note
// that the sync protocol buffer doesn't contain all SerializedNavigationEntry
// fields. |tab| must not be null.
void SetSessionTabFromSyncData(const sync_pb::SessionTab& sync_data,
                               base::Time timestamp,
                               sessions::SessionTab* tab);

// Convert |tab| into its sync protocol buffer equivalent. Uses
// SerializedNavigationEntry::ToSyncData to convert |navigations|. Note that the
// protocol buffer doesn't contain all SerializedNavigationEntry fields, and
// that the returned protocol buffer doesn't have any favicon data.
// |browser_type| needs to be provided separately because its (in local terms) a
// property of the window.
sync_pb::SessionTab SessionTabToSyncData(
    const sessions::SessionTab& tab,
    std::optional<sync_pb::SyncEnums::BrowserType> browser_type);

// A Sync wrapper for a SessionWindow.
struct SyncedSessionWindow {
  SyncedSessionWindow();

  SyncedSessionWindow(const SyncedSessionWindow&) = delete;
  SyncedSessionWindow& operator=(const SyncedSessionWindow&) = delete;

  ~SyncedSessionWindow();

  // Convert this object into its sync protocol buffer equivalent.
  sync_pb::SessionWindow ToSessionWindowProto() const;

  // Type of the window. See session_specifics.proto.
  sync_pb::SyncEnums::BrowserType window_type;

  // The SessionWindow this object wraps.
  sessions::SessionWindow wrapped_window;
};

// Defines a synced session for use by session sync. A synced session is a
// list of windows along with a unique session identifer (tag) and meta-data
// about the device being synced.
// TODO(crbug.com/40879579): Change struct to class to follow style guides.
struct SyncedSession {
 public:
  SyncedSession();

  SyncedSession(const SyncedSession&) = delete;
  SyncedSession& operator=(const SyncedSession&) = delete;

  ~SyncedSession();

  void SetSessionTag(const std::string& session_tag);
  const std::string& GetSessionTag() const;

  void SetSessionName(const std::string& session_name);
  const std::string& GetSessionName() const;

  // The timestamp when this session was started, i.e. when the user signed in
  // or turned on the sessions data type. Only populated for sessions started in
  // M130 or later.
  void SetStartTime(base::Time start_time);
  std::optional<base::Time> GetStartTime() const;

  void SetModifiedTime(const base::Time& modified_time);
  const base::Time& GetModifiedTime() const;

  // Map of windows that make up this session.
  std::map<SessionID, std::unique_ptr<SyncedSessionWindow>> windows;

  // Convert this object to its protocol buffer equivalent. Shallow conversion,
  // does not create SessionTab protobufs.
  sync_pb::SessionHeader ToSessionHeaderProto() const;

  void SetDeviceTypeAndFormFactor(
      const sync_pb::SyncEnums::DeviceType& local_device_type,
      const syncer::DeviceInfo::FormFactor& local_device_form_factor);

  syncer::DeviceInfo::FormFactor GetDeviceFormFactor() const;

 private:
  // Unique tag for each session.
  std::string session_tag_;

  // User-visible name
  std::string session_name_;

  // The timestamp when this session was started, i.e. when the user signed in
  // or turned on the sessions data type. Only populated for sessions started in
  // M130 or later.
  std::optional<base::Time> start_time_;

  // Last time this session was modified remotely. This is the max of the header
  // and all children tab mtimes.
  base::Time modified_time_;

  // Type of device this session is from.
  // It's used only to populate deprecated device_type by
  // ToSessionHeaderProto().
  sync_pb::SyncEnums::DeviceType device_type;

  // Form Factor of device this session is from.
  syncer::DeviceInfo::FormFactor device_form_factor =
      syncer::DeviceInfo::FormFactor::kUnknown;
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_SYNCED_SESSION_H_
