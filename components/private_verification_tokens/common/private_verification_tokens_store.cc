// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_verification_tokens/common/private_verification_tokens_store.h"

#include <optional>
#include <string>

#include "base/barrier_closure.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/private_verification_tokens/common/private_verification_tokens_public_key.h"

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

void PrivateVerificationTokensStore::CacheKeys(
    std::vector<PrivateVerificationTokensPublicKey> keys) {
  for (auto const& k : keys) {
    public_keys_.try_emplace(k.etld_plus_one(), k);
  }
}

void PrivateVerificationTokensStore::CacheTokens(
    std::map<std::string, TokenWithId> tokens) {
  tokens_ = std::move(tokens);
}

void PrivateVerificationTokensStore::InitializeCache(
    base::OnceCallback<void()> cache_initialized_callback,
    bool file_exists) {
  base::OnceClosure on_initialized = base::BindOnce(
      &PrivateVerificationTokensStore::OnCacheInitialized,
      weak_ptr_factory_.GetWeakPtr(), std::move(cache_initialized_callback));

  if (file_exists) {
    // There is already a DB file, cache tokens and keys async.
    base::RepeatingClosure barrier =
        base::BarrierClosure(2, std::move(on_initialized));

    database_.AsyncCall(&PrivateVerificationTokensDatabase::GetKeys)
        .Then(base::BindOnce(&PrivateVerificationTokensStore::CacheKeys,
                             weak_ptr_factory_.GetWeakPtr())
                  .Then(barrier));
    database_.AsyncCall(&PrivateVerificationTokensDatabase::GetTokensFromEach)
        .Then(base::BindOnce(&PrivateVerificationTokensStore::CacheTokens,
                             weak_ptr_factory_.GetWeakPtr())
                  .Then(barrier));
  } else {
    std::move(on_initialized).Run();
  }
}

void PrivateVerificationTokensStore::OnCacheInitialized(
    base::OnceCallback<void()> callback) {
  initialized_ = true;
  std::move(callback).Run();
}

const std::map<std::string, PrivateVerificationTokensPublicKey>&
PrivateVerificationTokensStore::public_keys() const {
  return public_keys_;
}

const std::map<std::string, TokenWithId>&
PrivateVerificationTokensStore::tokens() const {
  return tokens_;
}

PrivateVerificationTokensStore::~PrivateVerificationTokensStore() = default;

}  // namespace private_verification_tokens
