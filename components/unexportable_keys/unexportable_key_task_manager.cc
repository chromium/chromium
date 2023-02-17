// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/unexportable_key_task_manager.h"

#include <memory>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner_thread_mode.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/token.h"
#include "components/unexportable_keys/background_long_task_scheduler.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/ref_counted_unexportable_signing_key.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_tasks.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace unexportable_keys {

namespace {
scoped_refptr<RefCountedUnexportableSigningKey> MakeSigningKeyRefCounted(
    const UnexportableKeyId& key_id,
    std::unique_ptr<crypto::UnexportableSigningKey> key) {
  if (!key) {
    return nullptr;
  }

  return base::MakeRefCounted<RefCountedUnexportableSigningKey>(std::move(key),
                                                                key_id);
}
}  // namespace

UnexportableKeyTaskManager::UnexportableKeyTaskManager()
    : task_scheduler_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
          base::SingleThreadTaskRunnerThreadMode::
              DEDICATED  // Using a dedicated thread to run long and blocking
                         // TPM tasks.
          )) {}

UnexportableKeyTaskManager::~UnexportableKeyTaskManager() = default;

void UnexportableKeyTaskManager::GenerateSigningKeySlowlyAsync(
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(scoped_refptr<RefCountedUnexportableSigningKey>)>
        callback) {
  std::unique_ptr<crypto::UnexportableKeyProvider> key_provider =
      crypto::GetUnexportableKeyProvider();

  if (!key_provider) {
    std::move(callback).Run(nullptr);
    return;
  }

  auto task = std::make_unique<GenerateKeyTask>(
      std::move(key_provider), acceptable_algorithms,
      base::BindOnce(&MakeSigningKeyRefCounted,
                     UnexportableKeyId(base::Token::CreateRandom()))
          .Then(std::move(callback)));
  task_scheduler_.PostTask(std::move(task), priority);
}

void UnexportableKeyTaskManager::FromWrappedSigningKeySlowlyAsync(
    base::span<const uint8_t> wrapped_key,
    const UnexportableKeyId& key_id,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(scoped_refptr<RefCountedUnexportableSigningKey>)>
        callback) {
  std::unique_ptr<crypto::UnexportableKeyProvider> key_provider =
      crypto::GetUnexportableKeyProvider();

  if (!key_provider) {
    std::move(callback).Run(nullptr);
    return;
  }

  auto task = std::make_unique<FromWrappedKeyTask>(
      std::move(key_provider), wrapped_key,
      base::BindOnce(&MakeSigningKeyRefCounted, key_id)
          .Then(std::move(callback)));
  task_scheduler_.PostTask(std::move(task), priority);
}

void UnexportableKeyTaskManager::SignSlowlyAsync(
    scoped_refptr<RefCountedUnexportableSigningKey> signing_key,
    base::span<const uint8_t> data,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(absl::optional<std::vector<uint8_t>>)> callback) {
  // TODO(b/263249728): deduplicate tasks with the same parameters.
  // TODO(b/263249728): implement a cache of recent signings.
  auto task = std::make_unique<SignTask>(std::move(signing_key), data,
                                         std::move(callback));
  task_scheduler_.PostTask(std::move(task), priority);
}

}  // namespace unexportable_keys
