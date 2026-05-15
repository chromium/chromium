// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_verification_tokens/common/private_verification_tokens_store.h"

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"

namespace private_verification_tokens {

std::unique_ptr<PrivateVerificationTokensStore>
PrivateVerificationTokensStore::Create(base::FilePath path_to_database) {
  if (path_to_database.empty()) {
    return nullptr;
  }
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  auto sequence_bound_database =
      PrivateVerificationTokensDatabase::CreateSequenceBound(
          task_runner, std::move(path_to_database));
  DCHECK(!sequence_bound_database.is_null());
  return base::WrapUnique(
      new PrivateVerificationTokensStore(std::move(sequence_bound_database)));
}

PrivateVerificationTokensStore::PrivateVerificationTokensStore(
    base::SequenceBound<PrivateVerificationTokensDatabase> database)
    : database_(std::move(database)) {}

PrivateVerificationTokensStore::~PrivateVerificationTokensStore() = default;

}  // namespace private_verification_tokens
