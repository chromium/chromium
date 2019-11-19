// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_BROWSER_SYNC_CLIENT_H_
#define COMPONENTS_BROWSER_SYNC_BROWSER_SYNC_CLIENT_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/model/model_type_controller_delegate.h"

class BookmarkUndoService;

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace favicon {
class FaviconService;
}  // namespace favicon

namespace history {
class HistoryService;
}  // namespace history

namespace send_tab_to_self {
class SendTabToSelfSyncService;
}  // namespace send_tab_to_self

namespace sync_sessions {
class SessionSyncService;
}  // namespace sync_sessions

namespace syncer {
class DeviceInfoSyncService;
class ModelTypeStoreService;
}  // namespace syncer

namespace browser_sync {

// Extension to interface syncer::SyncClient to bundle dependencies that
// sync-the-feature requires for datatypes common to all platforms.
// Note: on some platforms, getters might return nullptr. Callers are expected
// to handle these scenarios gracefully.
class BrowserSyncClient : public syncer::SyncClient {
 public:
  BrowserSyncClient();
  ~BrowserSyncClient() override;

  base::FilePath GetSyncDataPath() final;
  virtual syncer::ModelTypeStoreService* GetModelTypeStoreService() = 0;

  // Returns a weak pointer to the ModelTypeControllerDelegate specified by
  // |type|. Weak pointer may be unset if service is already destroyed.
  virtual base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetControllerDelegateForModelType(syncer::ModelType type) = 0;

  // DataType specific service getters.
  virtual syncer::DeviceInfoSyncService* GetDeviceInfoSyncService() = 0;
  virtual bookmarks::BookmarkModel* GetBookmarkModel() = 0;
  virtual favicon::FaviconService* GetFaviconService() = 0;
  virtual history::HistoryService* GetHistoryService() = 0;
  virtual sync_sessions::SessionSyncService* GetSessionSyncService() = 0;
  virtual send_tab_to_self::SendTabToSelfSyncService*
  GetSendTabToSelfSyncService() = 0;
  virtual BookmarkUndoService* GetBookmarkUndoService() = 0;
  virtual base::RepeatingClosure GetPasswordStateChangedCallback() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserSyncClient);
};

}  // namespace browser_sync

#endif  // COMPONENTS_BROWSER_SYNC_BROWSER_SYNC_CLIENT_H_
