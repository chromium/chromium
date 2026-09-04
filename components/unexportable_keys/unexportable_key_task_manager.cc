// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/unexportable_key_task_manager.h"

#include <memory>
#include <optional>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner_thread_mode.h"
#include "base/task/task_traits.h"
#include "base/types/expected.h"
#include "components/unexportable_keys/background_long_task_scheduler.h"
#include "components/unexportable_keys/background_task_origin.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/background_task_type.h"
#include "components/unexportable_keys/features.h"
#include "components/unexportable_keys/ref_counted_unexportable_key.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_tasks.h"
#include "crypto/sign.h"
#include "crypto/unexportable_key.h"

namespace unexportable_keys {

namespace {

constexpr std::string_view kBaseTaskResultHistogramName =
    "Crypto.UnexportableKeys.BackgroundTaskResult";
constexpr std::string_view kBaseTaskRetriesHistogramName =
    "Crypto.UnexportableKeys.BackgroundTaskRetries";
constexpr size_t kSignTaskMaxRetries = 3;
constexpr size_t kCertifyTaskMaxRetries = 3;

template <class CallbackReturnType>
void ReportResultMetrics(BackgroundTaskType task_type,
                         BackgroundTaskOrigin task_origin,
                         const ServiceErrorOr<CallbackReturnType>& result,
                         size_t retry_count) {
  ServiceError error_for_metrics =
      result.has_value() ? kNoServiceErrorForMetrics : result.error();
  std::string_view task_type_suffix =
      GetBackgroundTaskTypeSuffixForHistograms(task_type);
  std::string_view success_suffix =
      result.has_value() ? ".Success" : ".Failure";

  base::UmaHistogramEnumeration(
      base::StrCat({kBaseTaskResultHistogramName, task_type_suffix}),
      error_for_metrics);
  base::UmaHistogramEnumeration(
      base::StrCat({kBaseTaskResultHistogramName,
                    GetBackgroundTaskOriginSuffixForHistograms(task_origin),
                    task_type_suffix}),
      error_for_metrics);
  base::UmaHistogramExactLinear(
      base::StrCat(
          {kBaseTaskRetriesHistogramName, task_type_suffix, success_suffix}),
      retry_count, /*exclusive_max=*/10);
}

// Returns a new callback that reports result metrics.
template <class CallbackReturnType>
base::OnceCallback<void(const ServiceErrorOr<CallbackReturnType>&, size_t)>
CreateMetricsCallback(BackgroundTaskType task_type,
                      BackgroundTaskOrigin task_origin) {
  return base::BindOnce(&ReportResultMetrics<CallbackReturnType>, task_type,
                        task_origin);
}

}  // namespace

UnexportableKeyTaskManager::UnexportableKeyTaskManager() = default;

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

void UnexportableKeyTaskManager::GetAllKeysForGarbageCollectionSlowlyAsync(
    BackgroundTaskOrigin origin,
    crypto::UnexportableKeyProvider::Config config,
    BackgroundTaskPriority priority,
    base::OnceCallback<
        void(ServiceErrorOr<
             std::vector<scoped_refptr<RefCountedUnexportableSigningKey>>>)>
        callback) {
  auto metrics_callback = CreateMetricsCallback<
      std::vector<scoped_refptr<RefCountedUnexportableSigningKey>>>(
      BackgroundTaskType::kGetAllKeys, origin);

  std::unique_ptr<crypto::UnexportableKeyProvider> key_provider =
      GetUnexportableKeyProvider(std::move(config));

  if (!key_provider) {
    std::move(metrics_callback)
        .Run(base::unexpected(ServiceError::kNoKeyProvider),
             /*retry_count=*/0);
    std::move(callback).Run(base::unexpected(ServiceError::kNoKeyProvider));
    return;
  }

  if (!key_provider->AsStatefulUnexportableKeyProvider()) {
    std::move(metrics_callback)
        .Run(base::unexpected(ServiceError::kOperationNotSupported),
             /*retry_count=*/0);
    std::move(callback).Run(
        base::unexpected(ServiceError::kOperationNotSupported));
    return;
  }

  auto task = std::make_unique<GetAllKeysTask>(std::move(key_provider),
                                               priority, std::move(callback),
                                               std::move(metrics_callback));
  task_scheduler_.PostTask(std::move(task));
}

void UnexportableKeyTaskManager::GenerateSigningKeySlowlyAsync(
    BackgroundTaskOrigin origin,
    crypto::UnexportableKeyProvider::Config config,
    base::span<const crypto::sign::SignatureKind> acceptable_algorithms,
    BackgroundTaskPriority priority,
    base::OnceCallback<
        void(ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>)>
        callback) {
  auto metrics_callback =
      CreateMetricsCallback<scoped_refptr<RefCountedUnexportableSigningKey>>(
          BackgroundTaskType::kGenerateKey, origin);

  std::unique_ptr<crypto::UnexportableKeyProvider> key_provider =
      GetUnexportableKeyProvider(std::move(config));

  if (!key_provider) {
    std::move(metrics_callback)
        .Run(base::unexpected(ServiceError::kNoKeyProvider), /*retry_count=*/0);
    std::move(callback).Run(base::unexpected(ServiceError::kNoKeyProvider));
    return;
  }

  auto task = std::make_unique<GenerateKeyTask>(
      std::move(key_provider), acceptable_algorithms, priority,
      std::move(callback), std::move(metrics_callback));
  task_scheduler_.PostTask(std::move(task));
}

void UnexportableKeyTaskManager::FromWrappedSigningKeySlowlyAsync(
    BackgroundTaskOrigin origin,
    crypto::UnexportableKeyProvider::Config config,
    base::span<const uint8_t> wrapped_key,
    BackgroundTaskPriority priority,
    base::OnceCallback<
        void(ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>)>
        callback) {
  auto metrics_callback =
      CreateMetricsCallback<scoped_refptr<RefCountedUnexportableSigningKey>>(
          BackgroundTaskType::kFromWrappedKey, origin);

  std::unique_ptr<crypto::UnexportableKeyProvider> key_provider =
      GetUnexportableKeyProvider(std::move(config));

  if (!key_provider) {
    std::move(metrics_callback)
        .Run(base::unexpected(ServiceError::kNoKeyProvider), /*retry_count=*/0);
    std::move(callback).Run(base::unexpected(ServiceError::kNoKeyProvider));
    return;
  }

  auto task = std::make_unique<FromWrappedKeyTask>(
      std::move(key_provider), wrapped_key, priority, std::move(callback),
      std::move(metrics_callback));
  task_scheduler_.PostTask(std::move(task));
}

void UnexportableKeyTaskManager::SignSlowlyAsync(
    BackgroundTaskType task_type,
    BackgroundTaskOrigin origin,
    scoped_refptr<RefCountedUnexportableSigningKey> signing_key,
    base::span<const uint8_t> data,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<std::vector<uint8_t>>)> callback) {
  CHECK(task_type == BackgroundTaskType::kSign ||
        task_type == BackgroundTaskType::kSignWithAttestationKey);
  auto metrics_callback =
      CreateMetricsCallback<std::vector<uint8_t>>(task_type, origin);

  // TODO(alexilin): convert this to a CHECK().
  if (!signing_key) {
    std::move(metrics_callback)
        .Run(base::unexpected(ServiceError::kKeyNotFound), /*retry_count=*/0);
    std::move(callback).Run(base::unexpected(ServiceError::kKeyNotFound));
    return;
  }

  // TODO(b/263249728): deduplicate tasks with the same parameters.
  // TODO(b/263249728): implement a cache of recent signings.
  auto task = std::make_unique<SignTask>(
      std::move(signing_key), data, priority, task_type, kSignTaskMaxRetries,
      std::move(callback), std::move(metrics_callback));
  task_scheduler_.PostTask(std::move(task));
}

void UnexportableKeyTaskManager::DeleteKeysSlowlyAsync(
    BackgroundTaskOrigin origin,
    crypto::UnexportableKeyProvider::Config config,
    std::vector<scoped_refptr<RefCountedUnexportableSigningKey>> keys,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<size_t>)> callback) {
  auto metrics_callback =
      CreateMetricsCallback<size_t>(BackgroundTaskType::kDeleteKeys, origin);

  std::unique_ptr<crypto::UnexportableKeyProvider> key_provider =
      GetUnexportableKeyProvider(std::move(config));

  if (!key_provider) {
    std::move(metrics_callback)
        .Run(base::unexpected(ServiceError::kNoKeyProvider), /*retry_count=*/0);
    std::move(callback).Run(base::unexpected(ServiceError::kNoKeyProvider));
    return;
  }

  if (!key_provider->AsStatefulUnexportableKeyProvider()) {
    std::move(metrics_callback)
        .Run(base::unexpected(ServiceError::kOperationNotSupported),
             /*retry_count=*/0);
    std::move(callback).Run(
        base::unexpected(ServiceError::kOperationNotSupported));
    return;
  }

  auto task = std::make_unique<DeleteKeysTask>(
      std::move(key_provider), std::move(keys), priority, std::move(callback),
      std::move(metrics_callback));
  task_scheduler_.PostTask(std::move(task));
}

void UnexportableKeyTaskManager::DeleteAllKeysSlowlyAsync(
    BackgroundTaskOrigin origin,
    crypto::UnexportableKeyProvider::Config config,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<size_t>)> callback) {
  auto metrics_callback =
      CreateMetricsCallback<size_t>(BackgroundTaskType::kDeleteAllKeys, origin);

  std::unique_ptr<crypto::UnexportableKeyProvider> key_provider =
      GetUnexportableKeyProvider(std::move(config));

  if (!key_provider) {
    std::move(metrics_callback)
        .Run(base::unexpected(ServiceError::kNoKeyProvider), /*retry_count=*/0);
    std::move(callback).Run(base::unexpected(ServiceError::kNoKeyProvider));
    return;
  }

  if (!key_provider->AsStatefulUnexportableKeyProvider()) {
    std::move(metrics_callback)
        .Run(base::unexpected(ServiceError::kOperationNotSupported),
             /*retry_count=*/0);
    std::move(callback).Run(
        base::unexpected(ServiceError::kOperationNotSupported));
    return;
  }

  auto task = std::make_unique<DeleteAllKeysTask>(std::move(key_provider),
                                                  priority, std::move(callback),
                                                  std::move(metrics_callback));
  task_scheduler_.PostTask(std::move(task));
}

void UnexportableKeyTaskManager::GenerateAttestationKeySlowlyAsync(
    BackgroundTaskOrigin origin,
    crypto::UnexportableKeyProvider::Config config,
    base::span<const crypto::sign::SignatureKind> acceptable_algorithms,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(
        ServiceErrorOr<scoped_refptr<RefCountedUnexportableAttestationKey>>)>
        callback) {
  auto metrics_callback = CreateMetricsCallback<
      scoped_refptr<RefCountedUnexportableAttestationKey>>(
      BackgroundTaskType::kGenerateAttestationKey, origin);

  std::unique_ptr<crypto::UnexportableKeyProvider> key_provider =
      GetUnexportableKeyProvider(std::move(config));

  if (!key_provider) {
    std::move(metrics_callback)
        .Run(base::unexpected(ServiceError::kNoKeyProvider), /*retry_count=*/0);
    std::move(callback).Run(base::unexpected(ServiceError::kNoKeyProvider));
    return;
  }

  auto task = std::make_unique<GenerateAttestationKeyTask>(
      std::move(key_provider), acceptable_algorithms, priority,
      std::move(callback), std::move(metrics_callback));
  task_scheduler_.PostTask(std::move(task));
}

void UnexportableKeyTaskManager::FromWrappedAttestationKeySlowlyAsync(
    BackgroundTaskOrigin origin,
    crypto::UnexportableKeyProvider::Config config,
    base::span<const uint8_t> wrapped_key,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(
        ServiceErrorOr<scoped_refptr<RefCountedUnexportableAttestationKey>>)>
        callback) {
  auto metrics_callback = CreateMetricsCallback<
      scoped_refptr<RefCountedUnexportableAttestationKey>>(
      BackgroundTaskType::kFromWrappedAttestationKey, origin);

  std::unique_ptr<crypto::UnexportableKeyProvider> key_provider =
      GetUnexportableKeyProvider(std::move(config));

  if (!key_provider) {
    std::move(metrics_callback)
        .Run(base::unexpected(ServiceError::kNoKeyProvider), /*retry_count=*/0);
    std::move(callback).Run(base::unexpected(ServiceError::kNoKeyProvider));
    return;
  }

  auto task = std::make_unique<FromWrappedAttestationKeyTask>(
      std::move(key_provider), wrapped_key, priority, std::move(callback),
      std::move(metrics_callback));
  task_scheduler_.PostTask(std::move(task));
}

void UnexportableKeyTaskManager::CertifySlowlyAsync(
    BackgroundTaskOrigin origin,
    scoped_refptr<RefCountedUnexportableAttestationKey> attestation_key,
    scoped_refptr<RefCountedUnexportableSigningKey> signing_key,
    base::span<const uint8_t> challenge,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<crypto::AttestationStatement>)>
        callback) {
  auto metrics_callback = CreateMetricsCallback<crypto::AttestationStatement>(
      BackgroundTaskType::kCertify, origin);

  if (!attestation_key || !signing_key) {
    std::move(metrics_callback)
        .Run(base::unexpected(ServiceError::kKeyNotFound), /*retry_count=*/0);
    std::move(callback).Run(base::unexpected(ServiceError::kKeyNotFound));
    return;
  }

  auto task = std::make_unique<CertifyTask>(
      std::move(attestation_key), std::move(signing_key), challenge, priority,
      kCertifyTaskMaxRetries, std::move(callback), std::move(metrics_callback));
  task_scheduler_.PostTask(std::move(task));
}

}  // namespace unexportable_keys
