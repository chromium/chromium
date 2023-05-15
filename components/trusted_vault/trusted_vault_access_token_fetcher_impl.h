// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_ACCESS_TOKEN_FETCHER_IMPL_H_
#define COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_ACCESS_TOKEN_FETCHER_IMPL_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/trusted_vault/trusted_vault_access_token_fetcher.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace trusted_vault {

class TrustedVaultAccessTokenFetcherFrontend;

// Must be created on the UI thread, but can be used (and cloned) on any
// sequence.
class TrustedVaultAccessTokenFetcherImpl
    : public TrustedVaultAccessTokenFetcher {
 public:
  explicit TrustedVaultAccessTokenFetcherImpl(
      base::WeakPtr<TrustedVaultAccessTokenFetcherFrontend> frontend);
  TrustedVaultAccessTokenFetcherImpl(
      const TrustedVaultAccessTokenFetcherImpl& other) = delete;
  TrustedVaultAccessTokenFetcherImpl& operator=(
      const TrustedVaultAccessTokenFetcherImpl& other) = delete;
  ~TrustedVaultAccessTokenFetcherImpl() override;

  // TrustedVaultAccessTokenFetcher implementation.
  void FetchAccessToken(const CoreAccountId& account_id,
                        TokenCallback callback) override;
  std::unique_ptr<TrustedVaultAccessTokenFetcher> Clone() override;

 private:
  TrustedVaultAccessTokenFetcherImpl(
      base::WeakPtr<TrustedVaultAccessTokenFetcherFrontend> frontend,
      scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner);

  base::WeakPtr<TrustedVaultAccessTokenFetcherFrontend> frontend_;
  scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner_;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_ACCESS_TOKEN_FETCHER_IMPL_H_
