// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_PUSH_CLIENT_CHANNEL_H_
#define COMPONENTS_INVALIDATION_IMPL_PUSH_CLIENT_CHANNEL_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/invalidation/impl/sync_system_resources.h"
#include "components/invalidation/public/invalidation_export.h"
#include "jingle/notifier/listener/push_client_observer.h"

namespace notifier {
class PushClient;
}  // namespace notifier

namespace syncer {

// A PushClientChannel is an implementation of NetworkChannel that
// routes messages through a PushClient.
class INVALIDATION_EXPORT PushClientChannel
    : public SyncNetworkChannel,
      public notifier::PushClientObserver {
 public:
  // |push_client| is guaranteed to be destroyed only when this object
  // is destroyed.
  explicit PushClientChannel(std::unique_ptr<notifier::PushClient> push_client);

  ~PushClientChannel() override;

  // invalidation::NetworkChannel implementation.
  void SendMessage(const std::string& message) override;
  void RequestDetailedStatus(
      base::Callback<void(const base::DictionaryValue&)> callback) override;

  // SyncNetworkChannel implementation.
  // If not connected, connects with the given credentials.  If
  // already connected, the next connection attempt will use the given
  // credentials.
  void UpdateCredentials(const CoreAccountId& account_id,
                         const std::string& token) override;
  int GetInvalidationClientType() override;

  // notifier::PushClient::Observer implementation.
  void OnNotificationsEnabled() override;
  void OnNotificationsDisabled(
      notifier::NotificationsDisabledReason reason) override;
  void OnIncomingNotification(
      const notifier::Notification& notification) override;

  const std::string& GetServiceContextForTest() const;

  int64_t GetSchedulingHashForTest() const;

  static std::string EncodeMessageForTest(const std::string& message,
                                          const std::string& service_context,
                                          int64_t scheduling_hash);

  static bool DecodeMessageForTest(const std::string& notification,
                                   std::string* message,
                                   std::string* service_context,
                                   int64_t* scheduling_hash);

 private:
  static void EncodeMessage(std::string* encoded_message,
                            const std::string& message,
                            const std::string& service_context,
                            int64_t scheduling_hash);
  static bool DecodeMessage(const std::string& data,
                            std::string* message,
                            std::string* service_context,
                            int64_t* scheduling_hash);
  std::unique_ptr<base::DictionaryValue> CollectDebugData() const;

  std::unique_ptr<notifier::PushClient> push_client_;
  std::string service_context_;
  int64_t scheduling_hash_;

  // This count is saved for displaying statatistics.
  int sent_messages_count_;

  DISALLOW_COPY_AND_ASSIGN(PushClientChannel);
};

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_IMPL_PUSH_CLIENT_CHANNEL_H_
