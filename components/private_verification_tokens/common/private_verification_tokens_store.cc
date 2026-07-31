// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_verification_tokens/common/private_verification_tokens_store.h"

#include <optional>
#include <string>

#include "base/barrier_closure.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "url/origin.h"

namespace private_verification_tokens {

std::unique_ptr<PrivateVerificationTokensStore>
PrivateVerificationTokensStore::Create(
    base::FilePath path_to_database,
    base::OnceCallback<void()> cache_initialized_callback) {
  if (path_to_database.empty()) {
    return nullptr;
  }
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  auto sequence_bound_database =
      PrivateVerificationTokensDatabase::CreateSequenceBound(task_runner,
                                                             path_to_database);
  DCHECK(!sequence_bound_database.is_null());
  return base::WrapUnique(new PrivateVerificationTokensStore(
      std::move(task_runner), std::move(sequence_bound_database),
      std::move(path_to_database), std::move(cache_initialized_callback)));
}

PrivateVerificationTokensStore::PrivateVerificationTokensStore(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::SequenceBound<PrivateVerificationTokensDatabase> database,
    base::FilePath path_to_database,
    base::OnceCallback<void()> cache_initialized_callback)
    : database_(std::move(database)) {
  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&base::PathExists, path_to_database),
      base::BindOnce(&PrivateVerificationTokensStore::InitializeCache,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(cache_initialized_callback)));
}

void PrivateVerificationTokensStore::CacheTokens(
    std::map<url::Origin, TokenWithId> tokens) {
  tokens_ = std::move(tokens);
}

void PrivateVerificationTokensStore::InitializeCache(
    base::OnceCallback<void()> cache_initialized_callback,
    bool file_exists) {
  base::OnceClosure on_initialized = base::BindOnce(
      &PrivateVerificationTokensStore::OnCacheInitialized,
      weak_ptr_factory_.GetWeakPtr(), std::move(cache_initialized_callback));

  if (file_exists) {
    database_.AsyncCall(&PrivateVerificationTokensDatabase::GetTokensFromEach)
        .Then(base::BindOnce(&PrivateVerificationTokensStore::CacheTokens,
                             weak_ptr_factory_.GetWeakPtr())
                  .Then(std::move(on_initialized)));
  } else {
    std::move(on_initialized).Run();
  }
}

void PrivateVerificationTokensStore::OnCacheInitialized(
    base::OnceCallback<void()> callback) {
  initialized_ = true;
  std::move(callback).Run();
}

const std::map<url::Origin, TokenWithId>&
PrivateVerificationTokensStore::tokens() const {
  return tokens_;
}

void PrivateVerificationTokensStore::DeleteAllTokens() {
  DeleteTokens(base::Time(), base::Time::Max(), std::nullopt,
               base::DoNothing());
  tokens_.clear();
}

void PrivateVerificationTokensStore::DeleteTokens(
    base::Time delete_begin,
    base::Time delete_end,
    std::optional<std::vector<url::Origin>> issuers,
    base::OnceClosure callback) {
  database_.AsyncCall(&PrivateVerificationTokensDatabase::DeleteTokens)
      .WithArgs(delete_begin, delete_end, std::move(issuers))
      .Then(base::BindOnce(&PrivateVerificationTokensStore::OnTokensDeleted,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(callback)));
}

void PrivateVerificationTokensStore::OnTokensDeleted(base::OnceClosure callback,
                                                     bool success) {
  database_.AsyncCall(&PrivateVerificationTokensDatabase::GetTokensFromEach)
      .Then(base::BindOnce(&PrivateVerificationTokensStore::CacheTokens,
                           weak_ptr_factory_.GetWeakPtr())
                .Then(std::move(callback)));
}

void PrivateVerificationTokensStore::StoreTokens(
    std::vector<PrivateVerificationTokensToken> tokens,
    base::OnceClosure callback) {
  database_.AsyncCall(&PrivateVerificationTokensDatabase::StoreTokens)
      .WithArgs(std::move(tokens))
      .Then(base::BindOnce(&PrivateVerificationTokensStore::OnTokensStored,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(callback)));
}

void PrivateVerificationTokensStore::OnTokensStored(base::OnceClosure callback,
                                                    bool success) {
  // If the database operation failed, don't bother refreshing the cache.
  if (!success) {
    std::move(callback).Run();
    return;
  }
  database_.AsyncCall(&PrivateVerificationTokensDatabase::GetTokensFromEach)
      .Then(base::BindOnce(&PrivateVerificationTokensStore::CacheTokens,
                           weak_ptr_factory_.GetWeakPtr())
                .Then(std::move(callback)));
}

PrivateVerificationTokensStore::~PrivateVerificationTokensStore() = default;

}  // namespace private_verification_tokens
