// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_ACCOUNT_MAPPER_H_
#define COMPONENTS_GCM_DRIVER_GCM_ACCOUNT_MAPPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/gcm_driver/gcm_app_handler.h"
#include "components/gcm_driver/gcm_client.h"
#include "google_apis/gcm/engine/account_mapping.h"

namespace base {
class Clock;
}

namespace gcm {

class GCMDriver;
extern const char kGCMAccountMapperAppId[];

// Class for mapping signed-in GAIA accounts to the GCM Device ID.
class GCMAccountMapper : public GCMAppHandler {
 public:
  // List of account mappings.
  using AccountMappings = std::vector<AccountMapping>;
  using DispatchMessageCallback =
      base::RepeatingCallback<void(const std::string& app_id,
                                   const IncomingMessage& message)>;

  explicit GCMAccountMapper(GCMDriver* gcm_driver);
  ~GCMAccountMapper() override;

  void Initialize(const AccountMappings& account_mappings,
                  const DispatchMessageCallback& callback);

  // Called by AccountTracker, when a new list of account tokens is available.
  // This will cause a refresh of account mappings and sending updates to GCM.
  void SetAccountTokens(
      const std::vector<GCMClient::AccountTokenInfo>& account_tokens);

  // Implementation of GCMAppHandler:
  void ShutdownHandler() override;
  void OnStoreReset() override;
  void OnMessage(const std::string& app_id,
                 const IncomingMessage& message) override;
  void OnMessagesDeleted(const std::string& app_id) override;
  void OnSendError(
      const std::string& app_id,
      const GCMClient::SendErrorDetails& send_error_details) override;
  void OnSendAcknowledged(const std::string& app_id,
                          const std::string& message_id) override;
  bool CanHandle(const std::string& app_id) const override;

 private:
  friend class GCMAccountMapperTest;

  typedef std::map<std::string, OutgoingMessage> OutgoingMessages;

  // Checks whether account mapper is ready to process new account tokens.
  bool IsReady();

  // Informs GCM of an added or refreshed account mapping.
  void SendAddMappingMessage(AccountMapping& account_mapping);

  // Informs GCM of a removed account mapping.
  void SendRemoveMappingMessage(AccountMapping& account_mapping);

  void CreateAndSendMessage(const AccountMapping& account_mapping);

  // Callback for sending a message.
  void OnSendFinished(const CoreAccountId& account_id,
                      const std::string& message_id,
                      GCMClient::Result result);

  // Gets a registration for account mapper from GCM.
  void GetRegistration();

  // Callback for registering with GCM.
  void OnRegisterFinished(const std::string& registration_id,
                          GCMClient::Result result);

  // Checks whether the update can be triggered now. If the current time is
  // within reasonable time (6 hours) of when the update is due, we want to
  // trigger the update immediately to take advantage of a fresh OAuth2 token.
  bool CanTriggerUpdate(const base::Time& last_update_time) const;

  // Checks whether last status change is older than a TTL of a message.
  bool IsLastStatusChangeOlderThanTTL(
      const AccountMapping& account_mapping) const;

  // Finds an account mapping in |accounts_| by |account_id|.
  AccountMapping* FindMappingByAccountId(const CoreAccountId& account_id);
  // Finds an account mapping in |accounts_| by |message_id|.
  // Returns iterator that can be used to delete the account.
  AccountMappings::iterator FindMappingByMessageId(
      const std::string& message_id);

  // Sets the clock for testing.
  void SetClockForTesting(base::Clock* clock);

  // GCMDriver owns GCMAccountMapper.
  GCMDriver* gcm_driver_;

  // Callback to GCMDriver to dispatch messages sent to Gaia ID.
  DispatchMessageCallback dispatch_message_callback_;

  // Clock for timestamping status changes.
  base::Clock* clock_;

  // Currnetly tracked account mappings.
  AccountMappings accounts_;

  std::vector<GCMClient::AccountTokenInfo> pending_account_tokens_;

  // GCM Registration ID of the account mapper.
  std::string registration_id_;

  bool initialized_;

  base::WeakPtrFactory<GCMAccountMapper> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GCMAccountMapper);
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_ACCOUNT_MAPPER_H_
