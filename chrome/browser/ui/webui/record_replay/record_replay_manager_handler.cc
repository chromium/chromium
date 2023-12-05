// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/record_replay/record_replay_manager_handler.h"
#include "components/record_replay/services/auth_token/public/cpp/auth_token_service_factory.h"
#include <string>

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"

RecordReplayManagerHandler::RecordReplayManagerHandler(
    Profile* profile,
    mojo::PendingReceiver<mojom::RecordReplayManagerHandler> receiver)
    : profile_(profile), receiver_(this, std::move(receiver)) {
  printf("RecordReplay [RUN-2886] ManagerHandler(%p)\n", this);
}

RecordReplayManagerHandler::~RecordReplayManagerHandler() = default;

void RecordReplayManagerHandler::SetManager(
    mojo::PendingRemote<mojom::RecordReplayManager> manager) {
  printf("RecordReplay [RUN-2886] ManagerHandler(%p)::SetManager()\n", this);
  manager_.Bind(std::move(manager));
}

void RecordReplayManagerHandler::ApiKeyReceived(const std::string& api_key) {
  printf("RecordReplay [RUN-2866] ManagerHandler(%p)::ApiKeyReceived(%s)\n", this, api_key.c_str());
  // Ideally this would use a mojo interface, but since both this code and the RecordReplayAuthTokenStore
  // are in the browser process, we can just call it directly.
  auth_token::RecordReplayAuthTokenServiceFactory::GetForBrowserContext(profile_)->SetToken(api_key);
}
