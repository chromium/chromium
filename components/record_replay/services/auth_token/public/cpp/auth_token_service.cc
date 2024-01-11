// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/services/auth_token/public/cpp/auth_token_service.h"
#include "content/public/browser/service_process_host.h"

namespace auth_token {

RecordReplayAuthTokenService::RecordReplayAuthTokenService() = default;
RecordReplayAuthTokenService::~RecordReplayAuthTokenService() = default;

void RecordReplayAuthTokenService::BindAuthTokenStore(
    mojo::PendingReceiver<mojom::RecordReplayAuthTokenStore> store) {

  auth_token_stores_.Add(this, std::move(store));
}

void RecordReplayAuthTokenService::SetToken(const std::string& token) {
  token_ = token;
  NotifyObservers();
}

void RecordReplayAuthTokenService::AddObserver(mojo::PendingRemote<mojom::RecordReplayAuthTokenStoreObserver> observer) {
  observers_.Add(std::move(observer));
  NotifyObservers();
}

void RecordReplayAuthTokenService::NotifyObservers() {
  for (auto& observer : observers_) {
    observer->OnRecordReplayAuthTokenChanged(token_);
  }
}

}  // namespace auth_token
