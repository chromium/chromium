// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/trusted_vault_access_token_fetcher_impl.h"

#include <memory>
#include <utility>

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "components/trusted_vault/trusted_vault_access_token_fetcher.h"
#include "components/trusted_vault/trusted_vault_access_token_fetcher_frontend.h"

namespace trusted_vault {

namespace {

// Attempts to fetch access token and immediately run |callback| if |frontend|
// isn't valid. Must be used on the UI thread.
void FetchAccessTokenOnUIThread(
    base::WeakPtr<TrustedVaultAccessTokenFetcherFrontend> frontend,
    const CoreAccountId& account_id,
    TrustedVaultAccessTokenFetcher::TokenCallback callback) {
  if (!frontend) {
    // This is likely to happen during the browser shutdown, leave request
    // hanging.
    return;
  }
  frontend->FetchAccessToken(account_id, std::move(callback));
}

}  // namespace

TrustedVaultAccessTokenFetcherImpl::TrustedVaultAccessTokenFetcherImpl(
    base::WeakPtr<TrustedVaultAccessTokenFetcherFrontend> frontend)
    : frontend_(frontend) {
  DCHECK(base::SequencedTaskRunner::HasCurrentDefault());
  ui_thread_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
}

TrustedVaultAccessTokenFetcherImpl::TrustedVaultAccessTokenFetcherImpl(
    base::WeakPtr<TrustedVaultAccessTokenFetcherFrontend> frontend,
    scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner)
    : frontend_(frontend), ui_thread_task_runner_(ui_thread_task_runner) {}

TrustedVaultAccessTokenFetcherImpl::~TrustedVaultAccessTokenFetcherImpl() =
    default;

void TrustedVaultAccessTokenFetcherImpl::FetchAccessToken(
    const CoreAccountId& account_id,
    TokenCallback callback) {
  ui_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(FetchAccessTokenOnUIThread, frontend_, account_id,
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

std::unique_ptr<TrustedVaultAccessTokenFetcher>
TrustedVaultAccessTokenFetcherImpl::Clone() {
  return base::WrapUnique(new TrustedVaultAccessTokenFetcherImpl(
      frontend_, ui_thread_task_runner_));
}

}  // namespace trusted_vault
