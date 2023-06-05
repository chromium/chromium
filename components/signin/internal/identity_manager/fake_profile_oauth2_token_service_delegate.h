// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_FAKE_PROFILE_OAUTH2_TOKEN_SERVICE_DELEGATE_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_FAKE_PROFILE_OAUTH2_TOKEN_SERVICE_DELEGATE_H_

#include <list>
#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

namespace network {
class SharedURLLoaderFactory;
}

class FakeProfileOAuth2TokenServiceDelegate
    : public ProfileOAuth2TokenServiceDelegate {
 public:
  FakeProfileOAuth2TokenServiceDelegate();

  FakeProfileOAuth2TokenServiceDelegate(
      const FakeProfileOAuth2TokenServiceDelegate&) = delete;
  FakeProfileOAuth2TokenServiceDelegate& operator=(
      const FakeProfileOAuth2TokenServiceDelegate&) = delete;

  ~FakeProfileOAuth2TokenServiceDelegate() override;

  std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      OAuth2AccessTokenConsumer* consumer) override;

  // Overriden to make sure it works on Android.
  bool RefreshTokenIsAvailable(const CoreAccountId& account_id) const override;

  std::vector<CoreAccountId> GetAccounts() const override;
  void RevokeAllCredentials() override;
  void LoadCredentials(const CoreAccountId& primary_account_id,
                       bool is_syncing) override;
  void UpdateCredentials(
      const CoreAccountId& account_id,
      const std::string& refresh_token
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      ,
      const std::vector<uint8_t>& wrapped_binding_key = std::vector<uint8_t>()
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
          ) override;
  void RevokeCredentials(const CoreAccountId& account_id) override;
  void ExtractCredentials(ProfileOAuth2TokenService* to_service,
                          const CoreAccountId& account_id) override;

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      const override;

  bool FixRequestErrorIfPossible() override;

  std::string GetRefreshToken(const CoreAccountId& account_id) const;

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  void set_fix_request_if_possible(bool value) {
    fix_request_if_possible_ = value;
  }

 private:
  void IssueRefreshTokenForUser(const CoreAccountId& account_id,
                                const std::string& token);

#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() override;
#endif

  // The account IDs, in the order they were first added.
  // A given account ID appears at most once in this list.
  std::list<CoreAccountId> account_ids_;

  // Maps account ids to tokens.
  std::map<CoreAccountId, std::string> refresh_tokens_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
  bool fix_request_if_possible_ = false;
};
#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_FAKE_PROFILE_OAUTH2_TOKEN_SERVICE_DELEGATE_H_
