// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_SESSION_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_SESSIONS_SESSION_SYNC_SERVICE_H_

#include <optional>
#include <string>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

namespace syncer {
class GlobalIdMapper;
class DataTypeControllerDelegate;
}  // namespace syncer

class GURL;
class SessionID;

namespace sync_sessions {

class OpenTabsUIDelegate;

// KeyedService responsible session sync (aka tab sync).
// This powers things like the history UI, where "Tabs from other devices"
// exists, as well as the uploading counterpart for other devices to see our
// local tabs.
class SessionSyncService : public KeyedService {
 public:
  SessionSyncService();

  SessionSyncService(const SessionSyncService&) = delete;
  SessionSyncService& operator=(const SessionSyncService&) = delete;

  ~SessionSyncService() override;

  virtual syncer::GlobalIdMapper* GetGlobalIdMapper() const = 0;

  // Return the active OpenTabsUIDelegate. If UserSelectableType::kTabs is not
  // enabled or not currently syncing, returns nullptr.
  virtual OpenTabsUIDelegate* GetOpenTabsUIDelegate() = 0;

  // Stores a screenshot for the local tab identified by `tab_id` in persisted
  // storage, and sends it to the sync server.
  virtual void AddTabScreenshot(SessionID tab_id,
                                std::string&& screenshot_data,
                                const GURL& url) = 0;

  // Reads a screenshot for the (local or remote) tab identified by
  // `session_tag` and `tab_id` from persisted storage and returns it to the
  // `callback`. If no screenshot is available or an error occurs while
  // reading it, the callback is invoked with a nullopt screenshot data.
  // Note: This special API is required since screenshots (as opposed to the
  // tabs themselves) are not stored in the in-memory model and are hence not
  // available through the normal read APIs.
  virtual void ReadTabScreenshot(
      const std::string& session_tag,
      SessionID tab_id,
      base::OnceCallback<void(std::optional<std::string>)> callback) = 0;

  // Allows client code to be notified when foreign sessions change.
  [[nodiscard]] virtual base::CallbackListSubscription
  SubscribeToForeignSessionsChanged(const base::RepeatingClosure& cb) = 0;

  virtual base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetControllerDelegate() = 0;
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_SESSION_SYNC_SERVICE_H_
