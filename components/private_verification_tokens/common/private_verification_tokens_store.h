// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_STORE_H_
#define COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_STORE_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "components/private_verification_tokens/common/private_verification_tokens_database.h"
#include "components/private_verification_tokens/common/private_verification_tokens_token.h"
#include "url/origin.h"

namespace private_verification_tokens {

// Uses database class to provide an async storage and retrieval
// interface.
class PrivateVerificationTokensStore {
 public:
  // Creates a sequenced task runner and moves database object there.
  static std::unique_ptr<PrivateVerificationTokensStore> Create(
      base::FilePath path_to_database,
      base::OnceCallback<void()> cache_initialized_callback);
  PrivateVerificationTokensStore(const PrivateVerificationTokensStore&) =
      delete;
  PrivateVerificationTokensStore& operator=(
      const PrivateVerificationTokensStore&) = delete;
  PrivateVerificationTokensStore(PrivateVerificationTokensStore&&) = delete;
  PrivateVerificationTokensStore& operator=(PrivateVerificationTokensStore&&) =
      delete;

  ~PrivateVerificationTokensStore();

  const std::map<url::Origin, TokenWithId>& tokens() const;
  bool is_initialized() const { return initialized_; }

  void DeleteAllTokens();
  void DeleteTokens(base::Time delete_begin,
                    base::Time delete_end,
                    std::optional<std::vector<url::Origin>> issuers,
                    base::OnceClosure callback);

  void StoreTokens(std::vector<PrivateVerificationTokensToken> tokens,
                   base::OnceClosure callback);

 private:
  explicit PrivateVerificationTokensStore(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      base::SequenceBound<PrivateVerificationTokensDatabase> database,
      base::FilePath path_to_database,
      base::OnceCallback<void()> cache_initialized_callback);

  void CacheTokens(std::map<url::Origin, TokenWithId> tokens);
  void OnCacheInitialized(base::OnceCallback<void()> callback);
  void InitializeCache(base::OnceCallback<void()> callback, bool file_exists);
  void OnTokensDeleted(base::OnceClosure callback, bool success);
  void OnTokensStored(base::OnceClosure callback, bool success);

  base::SequenceBound<PrivateVerificationTokensDatabase> database_;

  // Holds a single token for each issuer. These tokens are read from the
  // database.
  std::map<url::Origin, TokenWithId> tokens_;

  bool initialized_ = false;

  base::WeakPtrFactory<PrivateVerificationTokensStore> weak_ptr_factory_{this};
};

}  // namespace private_verification_tokens

#endif  // COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_STORE_H_
