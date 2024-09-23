// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/unexportable_key_task_manager.h"

#include <memory>
#include <optional>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner_thread_mode.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "components/unexportable_keys/background_long_task_scheduler.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/background_task_type.h"
#include "components/unexportable_keys/features.h"
#include "components/unexportable_keys/ref_counted_unexportable_signing_key.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_tasks.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"

namespace unexportable_keys {

namespace {

constexpr std::string_view kBaseTaskResultHistogramName =
    "Crypto.UnexportableKeys.BackgroundTaskResult";

ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>
MakeSigningKeyRefCounted(std::unique_ptr<crypto::UnexportableSigningKey> key) {
  if (!key) {
    return base::unexpected(ServiceError::kCryptoApiFailed);
  }

  return base::MakeRefCounted<RefCountedUnexportableSigningKey>(
      std::move(key), UnexportableKeyId());
}

ServiceErrorOr<std::vector<uint8_t>> OptionalToServiceErrorOr(
    std::optional<std::vector<uint8_t>> result) {
  if (!result) {
    return base::unexpected(ServiceError::kCryptoApiFailed);
  }

  return result.value();
}

template <class CallbackReturnType>
ServiceErrorOr<CallbackReturnType> ReportResultMetrics(
    BackgroundTaskType task_type,
    ServiceErrorOr<CallbackReturnType> result) {
  ServiceError error_for_metrics =
      result.has_value() ? kNoServiceErrorForMetrics : result.error();
  base::UmaHistogramEnumeration(
      base::StrCat({kBaseTaskResultHistogramName,
                    GetBackgroundTaskTypeSuffixForHistograms(task_type)}),
      error_for_metrics);
  return result;
}

// Returns a new callback that reports result metrics and then invokes the
// original `callback`.
template <class CallbackReturnType>
base::OnceCallback<void(ServiceErrorOr<CallbackReturnType>)>
WrapCallbackWithMetrics(
    BackgroundTaskType task_type,
    base::OnceCallback<void(ServiceErrorOr<CallbackReturnType>)> callback) {
  return base::BindOnce(&ReportResultMetrics<CallbackReturnType>, task_type)
      .Then(std::move(callback));
}

}  // namespace

UnexportableKeyTaskManager::UnexportableKeyTaskManager(
    crypto::UnexportableKeyProvider::Config config)
    : task_scheduler_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
          base::SingleThreadTaskRunnerThreadMode::
              DEDICATED  // Using a dedicated thread to run long and blocking
                         // TPM tasks.
          )),
      config_(std::move(config)) {}

UnexportableKeyTaskManager::~UnexportableKeyTaskManager() = default;

// static
std::unique_ptr<crypto::UnexportableKeyProvider>
UnexportableKeyTaskManager::GetUnexportableKeyProvider(
    crypto::UnexportableKeyProvider::Config config) {
  if (base::FeatureList::IsEnabled(
          kEnableBoundSessionCredentialsSoftwareKeysForManualTesting)) {
    return crypto::GetSoftwareUnsecureUnexportableKeyProvider();
  }

  return crypto::GetUnexportableKeyProvider(std::move(config));
}

void UnexportableKeyTaskManager::GenerateSigningKeySlowlyAsync(
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms,
    BackgroundTaskPriority priority,
    base::OnceCallback<
        void(ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>)>
        callback) {
  auto callback_wrapper = WrapCallbackWithMetrics(
      BackgroundTaskType::kGenerateKey, std::move(callback));

  std::unique_ptr<crypto::UnexportableKeyProvider> key_provider =
      GetUnexportableKeyProvider(config_);

  if (!key_provider) {
    std::move(callback_wrapper)
        .Run(base::unexpected(ServiceError::kNoKeyProvider));
    return;
  }

  if (!key_provider->SelectAlgorithm(acceptable_algorithms).has_value()) {
    std::move(callback_wrapper)
        .Run(base::unexpected(ServiceError::kAlgorithmNotSupported));
    return;
  }

  auto task = std::make_unique<GenerateKeyTask>(
      std::move(key_provider), acceptable_algorithms, priority,
      base::BindOnce(&MakeSigningKeyRefCounted)
          .Then(std::move(callback_wrapper)));
  task_scheduler_.PostTask(std::move(task));
}

void UnexportableKeyTaskManager::FromWrappedSigningKeySlowlyAsync(
    base::span<const uint8_t> wrapped_key,
    BackgroundTaskPriority priority,
    base::OnceCallback<
        void(ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>)>
        callback) {
  auto callback_wrapper = WrapCallbackWithMetrics(
      BackgroundTaskType::kFromWrappedKey, std::move(callback));

  std::unique_ptr<crypto::UnexportableKeyProvider> key_provider =
      GetUnexportableKeyProvider(config_);

  if (!key_provider) {
    std::move(callback_wrapper)
        .Run(base::unexpected(ServiceError::kNoKeyProvider));
    return;
  }

  auto task = std::make_unique<FromWrappedKeyTask>(
      std::move(key_provider), wrapped_key, priority,
      base::BindOnce(&MakeSigningKeyRefCounted)
          .Then(std::move(callback_wrapper)));
  task_scheduler_.PostTask(std::move(task));
}

void UnexportableKeyTaskManager::SignSlowlyAsync(
    scoped_refptr<RefCountedUnexportableSigningKey> signing_key,
    base::span<const uint8_t> data,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<std::vector<uint8_t>>)> callback) {
  auto callback_wrapper =
      WrapCallbackWithMetrics(BackgroundTaskType::kSign, std::move(callback));

  // TODO(alexilin): convert this to a CHECK().
  if (!signing_key) {
    std::move(callback_wrapper)
        .Run(base::unexpected(ServiceError::kKeyNotFound));
    return;
  }

  // TODO(b/263249728): deduplicate tasks with the same parameters.
  // TODO(b/263249728): implement a cache of recent signings.
  auto task =
      std::make_unique<SignTask>(std::move(signing_key), data, priority,
                                 base::BindOnce(&OptionalToServiceErrorOr)
                                     .Then(std::move(callback_wrapper)));
  task_scheduler_.PostTask(std::move(task));
}

}  // namespace unexportable_keys
