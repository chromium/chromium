// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/assistant_service_connection.h"

#include "chrome/browser/profiles/profile.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

constexpr const char kConnectionKey[] = "assistant_service_connection";

}  // namespace

AssistantServiceConnection::AssistantServiceConnection(Profile* profile)
    : service_(remote_.BindNewPipeAndPassReceiver(),
               profile->GetURLLoaderFactory()->Clone(),
               profile->GetPrefs()),
      profile_(profile) {
  profile_->AddObserver(this);
}

AssistantServiceConnection::~AssistantServiceConnection() {
  profile_->RemoveObserver(this);
}

// static
AssistantServiceConnection* AssistantServiceConnection::GetForProfile(
    Profile* profile) {
  auto* connection = static_cast<AssistantServiceConnection*>(
      profile->GetUserData(kConnectionKey));
  if (!connection) {
    auto new_connection = std::make_unique<AssistantServiceConnection>(profile);
    connection = new_connection.get();
    profile->SetUserData(kConnectionKey, std::move(new_connection));
  }
  return connection;
}

void AssistantServiceConnection::OnProfileWillBeDestroyed(Profile* profile) {
  // Clean up connection before the profile's PrefService is destroyed.
  profile->RemoveUserData(kConnectionKey);
  // |this| is deleted.
}
