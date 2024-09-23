// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_account_mapper.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/uuid.h"
#include "components/gcm_driver/gcm_driver_desktop.h"
#include "google_apis/gcm/engine/gcm_store.h"

namespace gcm {

namespace {

const char kGCMAccountMapperSenderId[] = "745476177629";
const char kGCMAccountMapperSendTo[] = "google.com";
const int kGCMAddMappingMessageTTL = 30 * 60;  // 0.5 hours in seconds.
const int kGCMRemoveMappingMessageTTL = 24 * 60 * 60;  // 1 day in seconds.
const int kGCMUpdateIntervalHours = 24;
// Because adding an account mapping dependents on a fresh OAuth2 token, we
// allow the update to happen earlier than update due time, if it is within
// the early start time to take advantage of that token.
const int kGCMUpdateEarlyStartHours = 6;
const char kRegistrationIdMessgaeKey[] = "id";
const char kTokenMessageKey[] = "t";
const char kAccountMessageKey[] = "a";
const char kRemoveAccountKey[] = "r";
const char kRemoveAccountValue[] = "1";
// Use to handle send to Gaia ID scenario:
const char kGCMSendToGaiaIdAppIdKey[] = "gcmb";


std::string GenerateMessageID() {
  return base::Uuid::GenerateRandomV4().AsLowercaseString();
}

}  // namespace

const char kGCMAccountMapperAppId[] = "com.google.android.gms";

GCMAccountMapper::GCMAccountMapper(GCMDriver* gcm_driver)
    : gcm_driver_(gcm_driver),
      clock_(base::DefaultClock::GetInstance()),
      initialized_(false) {}

GCMAccountMapper::~GCMAccountMapper() = default;

void GCMAccountMapper::Initialize(const AccountMappings& account_mappings,
                                  const DispatchMessageCallback& callback) {
  DCHECK(!initialized_);
  initialized_ = true;
  accounts_ = account_mappings;
  dispatch_message_callback_ = callback;
  GetRegistration();
}

void GCMAccountMapper::SetAccountTokens(
    const std::vector<GCMClient::AccountTokenInfo>& account_tokens) {
  DVLOG(1) << "GCMAccountMapper::SetAccountTokens called with "
           << account_tokens.size() << " accounts.";

  // If account mapper is not ready to handle tasks yet, save the latest
  // account tokens and return.
  if (!IsReady()) {
    pending_account_tokens_ = account_tokens;
    // If mapper is initialized, but still does not have registration ID,
    // maybe the registration gave up. Retrying in case.
    if (initialized_ && gcm_driver_->IsStarted())
      GetRegistration();
    return;
  }

  // Start from removing the old tokens, from all of the known accounts.
  for (auto iter = accounts_.begin(); iter != accounts_.end(); ++iter) {
    iter->access_token.clear();
  }

  // Update the internal collection of mappings with the new tokens.
  for (auto token_iter = account_tokens.begin();
       token_iter != account_tokens.end(); ++token_iter) {
    AccountMapping* account_mapping =
        FindMappingByAccountId(token_iter->account_id);
    if (!account_mapping) {
      AccountMapping new_mapping;
      new_mapping.status = AccountMapping::NEW;
      new_mapping.account_id = token_iter->account_id;
      new_mapping.access_token = token_iter->access_token;
      new_mapping.email = token_iter->email;
      accounts_.push_back(new_mapping);
    } else {
      // Since we got a token for an account, drop the remove message and treat
      // it as mapped.
      if (account_mapping->status == AccountMapping::REMOVING) {
        account_mapping->status = AccountMapping::MAPPED;
        account_mapping->status_change_timestamp = base::Time();
        account_mapping->last_message_id.clear();
      }

      account_mapping->email = token_iter->email;
      account_mapping->access_token = token_iter->access_token;
    }
  }
}

void GCMAccountMapper::ShutdownHandler() {
  initialized_ = false;
  accounts_.clear();
  registration_id_.clear();
  dispatch_message_callback_.Reset();
}

void GCMAccountMapper::OnStoreReset() {
  // TODO(crbug.com/40491756): Tell server to remove the mapping. But can't use
  // upstream GCM send for that since the store got reset.
  ShutdownHandler();
}

void GCMAccountMapper::OnMessage(const std::string& app_id,
                                 const IncomingMessage& message) {
  DCHECK_EQ(app_id, kGCMAccountMapperAppId);
  // TODO(fgorski): Report Send to Gaia ID failures using UMA.

  base::UmaHistogramBoolean("GCM.AccountMappingMessageReceived", true);

  if (dispatch_message_callback_.is_null()) {
    DVLOG(1) << "dispatch_message_callback_ missing in GCMAccountMapper";
    return;
  }

  auto it = message.data.find(kGCMSendToGaiaIdAppIdKey);
  if (it == message.data.end()) {
    DVLOG(1) << "Send to Gaia ID failure: Embedded app ID missing.";
    return;
  }

  std::string embedded_app_id = it->second;
  if (embedded_app_id.empty()) {
    DVLOG(1) << "Send to Gaia ID failure: Embedded app ID is empty.";
    return;
  }

  // Ensuring the message does not carry the embedded app ID.
  IncomingMessage new_message = message;
  new_message.data.erase(new_message.data.find(kGCMSendToGaiaIdAppIdKey));
  dispatch_message_callback_.Run(embedded_app_id, new_message);
}

void GCMAccountMapper::OnMessagesDeleted(const std::string& app_id) {
  // Account message does not expect messages right now.
}

void GCMAccountMapper::OnSendError(
    const std::string& app_id,
    const GCMClient::SendErrorDetails& send_error_details) {
  DCHECK_EQ(app_id, kGCMAccountMapperAppId);

  auto account_mapping_it =
      FindMappingByMessageId(send_error_details.message_id);

  if (account_mapping_it == accounts_.end())
    return;

  if (send_error_details.result != GCMClient::TTL_EXCEEDED) {
    DVLOG(1) << "Send error result different than TTL EXCEEDED: "
             << send_error_details.result << ". "
             << "Postponing the retry until a new batch of tokens arrives.";
    return;
  }

  if (account_mapping_it->status == AccountMapping::REMOVING) {
    // Another message to remove mapping can be sent immediately, because TTL
    // for those is one day. No need to back off.
    SendRemoveMappingMessage(*account_mapping_it);
  } else {
    if (account_mapping_it->status == AccountMapping::ADDING) {
      // There is no mapping established, so we can remove the entry.
      // Getting a fresh token will trigger a new attempt.
      gcm_driver_->RemoveAccountMapping(account_mapping_it->account_id);
      accounts_.erase(account_mapping_it);
    } else {
      // Account is already MAPPED, we have to wait for another token.
      account_mapping_it->last_message_id.clear();
      gcm_driver_->UpdateAccountMapping(*account_mapping_it);
    }
  }
}

void GCMAccountMapper::OnSendAcknowledged(const std::string& app_id,
                                          const std::string& message_id) {
  DCHECK_EQ(app_id, kGCMAccountMapperAppId);
  auto account_mapping_it = FindMappingByMessageId(message_id);

  DVLOG(1) << "OnSendAcknowledged with message ID: " << message_id;

  if (account_mapping_it == accounts_.end())
    return;

  // Here is where we advance a status of a mapping and persist or remove.
  if (account_mapping_it->status == AccountMapping::REMOVING) {
    // Message removing the account has been confirmed by the GCM, we can remove
    // all the information related to the account (from memory and store).
    gcm_driver_->RemoveAccountMapping(account_mapping_it->account_id);
    accounts_.erase(account_mapping_it);
  } else {
    // Mapping status is ADDING only when it is a first time mapping.
    DCHECK(account_mapping_it->status == AccountMapping::ADDING ||
           account_mapping_it->status == AccountMapping::MAPPED);

    // Account is marked as mapped with the current time.
    account_mapping_it->status = AccountMapping::MAPPED;
    account_mapping_it->status_change_timestamp = clock_->Now();
    // There is no pending message for the account.
    account_mapping_it->last_message_id.clear();

    gcm_driver_->UpdateAccountMapping(*account_mapping_it);
  }
}

bool GCMAccountMapper::CanHandle(const std::string& app_id) const {
  return app_id.compare(kGCMAccountMapperAppId) == 0;
}

bool GCMAccountMapper::IsReady() {
  return initialized_ && gcm_driver_->IsStarted() && !registration_id_.empty();
}

void GCMAccountMapper::SendAddMappingMessage(AccountMapping& account_mapping) {
  CreateAndSendMessage(account_mapping);
}

void GCMAccountMapper::SendRemoveMappingMessage(
    AccountMapping& account_mapping) {
  // We want to persist an account that is being removed as quickly as possible
  // as well as clean up the last message information.
  if (account_mapping.status != AccountMapping::REMOVING) {
    account_mapping.status = AccountMapping::REMOVING;
    account_mapping.status_change_timestamp = clock_->Now();
  }

  account_mapping.last_message_id.clear();

  gcm_driver_->UpdateAccountMapping(account_mapping);

  CreateAndSendMessage(account_mapping);
}

void GCMAccountMapper::CreateAndSendMessage(
    const AccountMapping& account_mapping) {
  OutgoingMessage outgoing_message;
  outgoing_message.id = GenerateMessageID();
  outgoing_message.data[kRegistrationIdMessgaeKey] = registration_id_;
  outgoing_message.data[kAccountMessageKey] = account_mapping.email;

  if (account_mapping.status == AccountMapping::REMOVING) {
    outgoing_message.time_to_live = kGCMRemoveMappingMessageTTL;
    outgoing_message.data[kRemoveAccountKey] = kRemoveAccountValue;
  } else {
    outgoing_message.data[kTokenMessageKey] = account_mapping.access_token;
    outgoing_message.time_to_live = kGCMAddMappingMessageTTL;
  }

  gcm_driver_->Send(kGCMAccountMapperAppId, kGCMAccountMapperSendTo,
                    outgoing_message,
                    base::BindOnce(&GCMAccountMapper::OnSendFinished,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   account_mapping.account_id));
}

void GCMAccountMapper::OnSendFinished(const CoreAccountId& account_id,
                                      const std::string& message_id,
                                      GCMClient::Result result) {
  // TODO(fgorski): Add another attempt, in case the QUEUE is not full.
  if (result != GCMClient::SUCCESS)
    return;

  AccountMapping* account_mapping = FindMappingByAccountId(account_id);
  DCHECK(account_mapping);

  // If we are dealing with account with status NEW, it is the first time
  // mapping, and we should mark it as ADDING.
  if (account_mapping->status == AccountMapping::NEW) {
    account_mapping->status = AccountMapping::ADDING;
    account_mapping->status_change_timestamp = clock_->Now();
  }

  account_mapping->last_message_id = message_id;

  gcm_driver_->UpdateAccountMapping(*account_mapping);
}

void GCMAccountMapper::GetRegistration() {
  DCHECK(registration_id_.empty());
  std::vector<std::string> sender_ids;
  sender_ids.push_back(kGCMAccountMapperSenderId);
  gcm_driver_->Register(kGCMAccountMapperAppId, sender_ids,
                        base::BindOnce(&GCMAccountMapper::OnRegisterFinished,
                                       weak_ptr_factory_.GetWeakPtr()));
}

void GCMAccountMapper::OnRegisterFinished(const std::string& registration_id,
                                          GCMClient::Result result) {
  if (result == GCMClient::SUCCESS)
    registration_id_ = registration_id;

  if (IsReady()) {
    if (!pending_account_tokens_.empty()) {
      SetAccountTokens(pending_account_tokens_);
      pending_account_tokens_.clear();
    }
  }
}

bool GCMAccountMapper::CanTriggerUpdate(
    const base::Time& last_update_time) const {
  return last_update_time +
             base::Hours(kGCMUpdateIntervalHours - kGCMUpdateEarlyStartHours) <
         clock_->Now();
}

bool GCMAccountMapper::IsLastStatusChangeOlderThanTTL(
    const AccountMapping& account_mapping) const {
  int ttl_seconds = account_mapping.status == AccountMapping::REMOVING ?
      kGCMRemoveMappingMessageTTL : kGCMAddMappingMessageTTL;
  return account_mapping.status_change_timestamp + base::Seconds(ttl_seconds) <
         clock_->Now();
}

AccountMapping* GCMAccountMapper::FindMappingByAccountId(
    const CoreAccountId& account_id) {
  for (auto iter = accounts_.begin(); iter != accounts_.end(); ++iter) {
    if (iter->account_id == account_id)
      return &*iter;
  }

  return nullptr;
}

GCMAccountMapper::AccountMappings::iterator
GCMAccountMapper::FindMappingByMessageId(const std::string& message_id) {
  for (auto iter = accounts_.begin(); iter != accounts_.end(); ++iter) {
    if (iter->last_message_id == message_id)
      return iter;
  }

  return accounts_.end();
}

void GCMAccountMapper::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

}  // namespace gcm
