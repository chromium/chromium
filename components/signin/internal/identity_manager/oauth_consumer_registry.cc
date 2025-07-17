// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/oauth_consumer_registry.h"

#include "base/notreached.h"
#include "google_apis/gaia/gaia_constants.h"

namespace {
constexpr char kSyncOAuthConsumerName[] = "sync";
}

namespace signin {

OAuthConsumer::OAuthConsumer(const std::string& name, const ScopeSet& scopes)
    : name_(name), scopes_(scopes) {
  CHECK(!name.empty());
  CHECK(!scopes.empty());
}

OAuthConsumer::~OAuthConsumer() = default;

std::string OAuthConsumer::GetName() const {
  return name_;
}

ScopeSet OAuthConsumer::GetScopes() const {
  return scopes_;
}

OAuthConsumer GetOAuthConsumerFromId(OAuthConsumerId oauth_consumer_id) {
  switch (oauth_consumer_id) {
    case OAuthConsumerId::kSync:
      return OAuthConsumer(/* name= */ kSyncOAuthConsumerName, /* scopes= */ {
                               GaiaConstants::kChromeSyncOAuth2Scope});
  }
  NOTREACHED();
}

}  // namespace signin
