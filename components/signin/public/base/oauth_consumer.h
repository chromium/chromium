
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_OAUTH_CONSUMER_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_OAUTH_CONSUMER_H_

#include <set>
#include <string>

namespace signin {

// TODO(crbug.com/425896213): Add std::less<> after removing scope_set.h
using ScopeSet = std::set<std::string>;

// Represents an OAuth consumer, identified by its name and the OAuth2 scopes it
// requires.
class OAuthConsumer final {
 public:
  OAuthConsumer(const std::string& name, const ScopeSet& scopes);

  OAuthConsumer() = delete;
  ~OAuthConsumer();

  OAuthConsumer(const OAuthConsumer&);
  OAuthConsumer(OAuthConsumer&&);

  OAuthConsumer& operator=(const OAuthConsumer&) = delete;
  OAuthConsumer& operator=(OAuthConsumer&&) = delete;

  std::string GetName() const;
  ScopeSet GetScopes() const;

 private:
  const std::string name_;
  const ScopeSet scopes_;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_OAUTH_CONSUMER_H_
