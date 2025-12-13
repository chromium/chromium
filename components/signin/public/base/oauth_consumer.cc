// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/oauth_consumer.h"

#include "base/check.h"

namespace signin {

OAuthConsumer::OAuthConsumer(const std::string& name, const ScopeSet& scopes)
    : name_(name), scopes_(scopes) {
  CHECK(!name.empty());
}

OAuthConsumer::~OAuthConsumer() = default;
OAuthConsumer::OAuthConsumer(const OAuthConsumer&) = default;
OAuthConsumer::OAuthConsumer(OAuthConsumer&&) = default;

std::string OAuthConsumer::GetName() const {
  return name_;
}

ScopeSet OAuthConsumer::GetScopes() const {
  return scopes_;
}

}  // namespace signin
