// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RECORD_REPLAY_SERVICES_AUTH_TOKEN_PUBLIC_CPP_AUTH_TOKEN_SERVICE_H_
#define COMPONENTS_RECORD_REPLAY_SERVICES_AUTH_TOKEN_PUBLIC_CPP_AUTH_TOKEN_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"
#include "components/record_replay/services/auth_token/public/mojom/auth_token.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace auth_token {

class RecordReplayAuthTokenService : public KeyedService, public mojom::RecordReplayAuthTokenStore {
 public:
  RecordReplayAuthTokenService();
  RecordReplayAuthTokenService(const RecordReplayAuthTokenService&) = delete;
  RecordReplayAuthTokenService& operator=(const RecordReplayAuthTokenService&) = delete;
  ~RecordReplayAuthTokenService() override;

  void BindAuthTokenStore(
      mojo::PendingReceiver<mojom::RecordReplayAuthTokenStore> store);

  // mojom::RecordReplayAuthTokenStore:
  void SetToken(const std::string& token) override;
  void AddObserver(mojo::PendingRemote<mojom::RecordReplayAuthTokenStoreObserver> observer) override;

private:
  mojo::ReceiverSet<mojom::RecordReplayAuthTokenStore> auth_token_stores_;

  std::string token_;

  void NotifyObservers();

  mojo::RemoteSet<mojom::RecordReplayAuthTokenStoreObserver> observers_;
};

}  // namespace auth_token

#endif  // COMPONENTS_RECORD_REPLAY_SERVICES_AUTH_TOKEN_PUBLIC_CPP_AUTH_TOKEN_SERVICE_H_
