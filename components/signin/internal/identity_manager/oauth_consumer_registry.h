// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_OAUTH_CONSUMER_REGISTRY_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_OAUTH_CONSUMER_REGISTRY_H_

#include <string>

#include "components/signin/public/identity_manager/oauth_consumer_ids.h"
#include "components/signin/public/identity_manager/scope_set.h"

namespace signin {

class OAuthConsumer {
 public:
  OAuthConsumer(const std::string& name, const ScopeSet& scopes);

  OAuthConsumer() = delete;
  ~OAuthConsumer();

  OAuthConsumer(const OAuthConsumer&) = delete;
  OAuthConsumer(OAuthConsumer&&) = delete;

  OAuthConsumer& operator=(const OAuthConsumer&) = delete;
  OAuthConsumer& operator=(OAuthConsumer&&) = delete;

  std::string GetName() const;
  ScopeSet GetScopes() const;

 private:
  const std::string name_;
  const ScopeSet scopes_;
};

OAuthConsumer GetOAuthConsumerFromId(OAuthConsumerId oauth_consumer_id);

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_OAUTH_CONSUMER_REGISTRY_H_
