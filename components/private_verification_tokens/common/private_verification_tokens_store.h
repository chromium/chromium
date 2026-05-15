// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_STORE_H_
#define COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_STORE_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/threading/sequence_bound.h"
#include "components/private_verification_tokens/common/private_verification_tokens_database.h"

namespace private_verification_tokens {

// Uses database class to provide an async storage and retrieval
// interface.
class PrivateVerificationTokensStore {
 public:
  // Creates a sequenced task runner and moves database object there.
  static std::unique_ptr<PrivateVerificationTokensStore> Create(
      base::FilePath path_to_database);
  PrivateVerificationTokensStore(const PrivateVerificationTokensStore&) =
      delete;
  PrivateVerificationTokensStore& operator=(
      const PrivateVerificationTokensStore&) = delete;
  PrivateVerificationTokensStore(PrivateVerificationTokensStore&&) = delete;
  PrivateVerificationTokensStore& operator=(PrivateVerificationTokensStore&&) =
      delete;

  ~PrivateVerificationTokensStore();

 private:
  explicit PrivateVerificationTokensStore(
      base::SequenceBound<PrivateVerificationTokensDatabase> database);
  base::SequenceBound<PrivateVerificationTokensDatabase> database_;
};

}  // namespace private_verification_tokens

#endif  // COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_STORE_H_
