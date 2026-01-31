// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_OAUTH_CONSUMER_REGISTRY_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_OAUTH_CONSUMER_REGISTRY_H_

#include "components/signin/public/base/oauth_consumer.h"
#include "components/signin/public/base/oauth_consumer_id.h"

namespace signin {

// Get OAuthConsumer for consumers that use dynamic scopes when requesting an
// access token.
OAuthConsumer GetOAuthConsumerForDynamicScopes(
    OAuthConsumerId oauth_consumer_id,
    const signin::ScopeSet& scopes);

// Registry for oauth consumers. Provides a way to get an OAuthConsumer
// instance from its OAuthConsumerId. This class already supports conversions
// for most consumers. Consumers that are not straight-forward to convert
// (because they get their scopes from finch for example) are converted in
// it's subclasses. Embedders of `IdentityManager` have their own subclasses of
// this class.
class OAuthConsumerRegistry {
 public:
  OAuthConsumerRegistry();
  virtual ~OAuthConsumerRegistry();

  OAuthConsumerRegistry(const OAuthConsumerRegistry&) = delete;
  OAuthConsumerRegistry(OAuthConsumerRegistry&&) = delete;

  OAuthConsumerRegistry& operator=(const OAuthConsumerRegistry&) = delete;
  OAuthConsumerRegistry& operator=(OAuthConsumerRegistry&&) = delete;

  OAuthConsumer GetOAuthConsumerFromId(OAuthConsumerId oauth_consumer_id) const;

 protected:
  virtual OAuthConsumer GetOAuthConsumerForEnterprisePlusAddress() const = 0;
  virtual OAuthConsumer GetOAuthConsumerForGlicUserStatus() const = 0;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_OAUTH_CONSUMER_REGISTRY_H_
