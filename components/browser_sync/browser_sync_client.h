// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_BROWSER_SYNC_CLIENT_H_
#define COMPONENTS_BROWSER_SYNC_BROWSER_SYNC_CLIENT_H_

#include "base/functional/callback_forward.h"
#include "components/sync/service/sync_client.h"

namespace consent_auditor {
class ConsentAuditor;
}  // namespace consent_auditor

namespace favicon {
class FaviconService;
}  // namespace favicon

namespace history {
class HistoryService;
}  // namespace history

class ReadingListModel;

namespace password_manager {
class PasswordReceiverService;
class PasswordSenderService;
}  // namespace password_manager

namespace send_tab_to_self {
class SendTabToSelfSyncService;
}  // namespace send_tab_to_self

namespace sync_preferences {
class PrefServiceSyncable;
}  // namespace sync_preferences

namespace sync_sessions {
class SessionSyncService;
}  // namespace sync_sessions

namespace syncer {
class DeviceInfoSyncService;
class ModelTypeStoreService;
class UserEventService;
}  // namespace syncer

namespace webauthn {
class PasskeyModel;
}  // namespace webauthn

namespace browser_sync {

// Extension to interface syncer::SyncClient to bundle dependencies that
// sync requires for datatypes common to all platforms.
// Note: on some platforms, getters might return nullptr. Callers are expected
// to handle these scenarios gracefully.
class BrowserSyncClient : public syncer::SyncClient {
 public:
  BrowserSyncClient() = default;

  BrowserSyncClient(const BrowserSyncClient&) = delete;
  BrowserSyncClient& operator=(const BrowserSyncClient&) = delete;

  ~BrowserSyncClient() override = default;

  virtual syncer::ModelTypeStoreService* GetModelTypeStoreService() = 0;

  // DataType specific service getters.
  virtual consent_auditor::ConsentAuditor* GetConsentAuditor() = 0;
  virtual syncer::DeviceInfoSyncService* GetDeviceInfoSyncService() = 0;
  virtual favicon::FaviconService* GetFaviconService() = 0;
  virtual history::HistoryService* GetHistoryService() = 0;
  virtual webauthn::PasskeyModel* GetPasskeyModel() = 0;
  virtual password_manager::PasswordReceiverService*
  GetPasswordReceiverService() = 0;
  virtual password_manager::PasswordSenderService*
  GetPasswordSenderService() = 0;
  virtual sync_preferences::PrefServiceSyncable* GetPrefServiceSyncable() = 0;
  virtual sync_sessions::SessionSyncService* GetSessionSyncService() = 0;
  virtual ReadingListModel* GetReadingListModel() = 0;
  virtual send_tab_to_self::SendTabToSelfSyncService*
  GetSendTabToSelfSyncService() = 0;
  virtual syncer::UserEventService* GetUserEventService() = 0;
};

}  // namespace browser_sync

#endif  // COMPONENTS_BROWSER_SYNC_BROWSER_SYNC_CLIENT_H_
