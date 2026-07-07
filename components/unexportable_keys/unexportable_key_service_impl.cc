// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/unexportable_key_service_impl.h"

#include <algorithm>
#include <array>
#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>
#include <variant>

#include "base/containers/map_util.h"
#include "base/containers/to_vector.h"
#include "base/containers/transparent_hash.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/function_ref.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/unexportable_keys/background_task_origin.h"
#include "components/unexportable_keys/features.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/unexportable_key.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace unexportable_keys {

namespace {

// The default list of signature algorithms to use for generating spare keys.
constexpr std::array kSpareKeyAlgorithms = {
    crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
    crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256,
};

// Delays the initial replenishment of the spare key pool during service
// creation. Hardware-backed key generation is extremely TPM/CPU intensive.
// Deferring this by 2 minutes ensures it does not compete for resources
// during the critical browser startup path.
constexpr base::TimeDelta kSpareKeyPoolDelay = base::Minutes(2);

// Creates a repeating key generation callback for the spare key pool,
// binding the task manager and background origin.
//
// The task_manager and config must outlive the callback as they are stored in
// the bind state by a reference.
template <typename KeyType>
base::RepeatingCallback<
    void(crypto::UnexportableKeyProvider::Config,
         base::span<const crypto::SignatureVerifier::SignatureAlgorithm>,
         base::OnceCallback<void(ServiceErrorOr<scoped_refptr<KeyType>>)>)>
CreateGenerateKeyCallbackForSparePool(UnexportableKeyTaskManager* task_manager,
                                      BackgroundTaskOrigin origin) {
  static_assert(std::same_as<KeyType, RefCountedUnexportableSigningKey> ||
                    std::same_as<KeyType, RefCountedUnexportableAttestationKey>,
                "Unsupported KeyType");

  return base::BindRepeating(
      [](UnexportableKeyTaskManager* task_manager, BackgroundTaskOrigin origin,
         crypto::UnexportableKeyProvider::Config config,
         base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
             algorithms,
         base::OnceCallback<void(ServiceErrorOr<scoped_refptr<KeyType>>)>
             callback) {
        if constexpr (std::same_as<KeyType, RefCountedUnexportableSigningKey>) {
          task_manager->GenerateSigningKeySlowlyAsync(
              origin, std::move(config), algorithms,
              BackgroundTaskPriority::kBestEffort, std::move(callback));
        } else if constexpr (std::same_as<
                                 KeyType,
                                 RefCountedUnexportableAttestationKey>) {
          task_manager->GenerateAttestationKeySlowlyAsync(
              origin, std::move(config), algorithms,
              BackgroundTaskPriority::kBestEffort, std::move(callback));
        }
      },
      base::Unretained(task_manager), origin);
}

using WrappedKeyAndTag = std::pair<std::vector<uint8_t>, std::string>;
using WrappedKeyAndTagView =
    std::pair<base::span<const uint8_t>, std::string_view>;

WrappedKeyAndTag Materialize(WrappedKeyAndTagView view) {
  auto [wrapped_key, tag] = view;
  return {base::ToVector(wrapped_key), std::string(tag)};
}

// Returns the application tag from the config on Mac if the provider supports
// stateful unexportable keys. Otherwise, returns an empty string.
std::string_view GetApplicationTag(
    const crypto::UnexportableKeyProvider::Config& config) {
#if BUILDFLAG(IS_MAC)
  if (UnexportableKeyServiceImpl::IsStatefulUnexportableKeyProviderSupported(
          config)) {
    return config.application_tag;
  }

  return "";
#else
  return "";
#endif  // BUILDFLAG(IS_MAC)
}

// Convenience method to create a `WrappedKeyAndTag` from a
// `RefCountedUnexportableKey`.
std::pair<std::vector<uint8_t>, std::string> GetWrappedKeyAndTag(
    const RefCountedUnexportableKey& key) {
  std::string tag;
  if (const crypto::StatefulKey* stateful_key = key.key().AsStatefulKey()) {
    tag = stateful_key->GetKeyTag();
  }

  return {key.key().GetWrappedKey(), std::move(tag)};
}

// Resolves the full UMA histogram name at compile-time based on the template
// `KeyType` parameter. Only supported key types are permitted to compile.
//
// LINT.IfChange(GetSpareKeyPoolHistogramName)
template <typename KeyType>
std::string GetSpareKeyPoolHistogramName(std::string_view suffix) {
  static_assert(std::same_as<KeyType, RefCountedUnexportableSigningKey> ||
                    std::same_as<KeyType, RefCountedUnexportableAttestationKey>,
                "Unsupported KeyType for spare key pool metrics");

  static constexpr std::string_view kSpareKeyPoolHistogramPrefix =
      "Crypto.UnexportableKeys.SparePool";
  return base::JoinString(
      {kSpareKeyPoolHistogramPrefix,
       std::same_as<KeyType, RefCountedUnexportableSigningKey> ? "Signing"
                                                               : "Attestation",
       suffix},
      ".");
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/net/histograms.xml:UnexportableKeysSpareKeyPoolType)

// Wraps the original key generation callback with latency metrics tracking.
// This records the duration from when the request was initiated until it is
// fulfilled (either from the spare pool or via hardware generation).
template <typename KeyIdType>
base::OnceCallback<void(ServiceErrorOr<KeyIdType>)>
WrapCallbackWithSpareKeyLatencyHistogram(
    base::OnceCallback<void(ServiceErrorOr<KeyIdType>)> callback) {
  // Only `UnexportableSigningKeyId` and `UnexportableAttestationKeyId` are
  // supported by the spare key pool.
  static_assert(std::same_as<KeyIdType, UnexportableSigningKeyId> ||
                std::same_as<KeyIdType, UnexportableAttestationKeyId>);

  using KeyType =
      std::conditional_t<std::same_as<KeyIdType, UnexportableSigningKeyId>,
                         RefCountedUnexportableSigningKey,
                         RefCountedUnexportableAttestationKey>;

  return base::BindOnce(
      [](base::TimeTicks start,
         base::OnceCallback<void(ServiceErrorOr<KeyIdType>)> cb,
         ServiceErrorOr<KeyIdType> result) {
        base::UmaHistogramMediumTimes(
            GetSpareKeyPoolHistogramName<KeyType>("RequestLatency"),
            base::TimeTicks::Now() - start);
        std::move(cb).Run(std::move(result));
      },
      base::TimeTicks::Now(), std::move(callback));
}

// Class holding either a `KeyIdType` or a list of callbacks waiting for the
// key creation.
template <typename KeyIdType>
class MaybePendingKeyId {
 public:
  using CallbackType = base::OnceCallback<void(ServiceErrorOr<KeyIdType>)>;
  using PendingCallbacks = std::vector<CallbackType>;
  using PendingCallbacksOrKeyId = std::variant<PendingCallbacks, KeyIdType>;

  // Constructs an instance holding a list of callbacks.
  MaybePendingKeyId() = default;
  MaybePendingKeyId(MaybePendingKeyId&&) = default;
  MaybePendingKeyId& operator=(MaybePendingKeyId&&) = default;

  // Constructs an instance holding `key_id`.
  explicit MaybePendingKeyId(KeyIdType key_id)
      : pending_callbacks_or_key_id_(key_id) {}

  ~MaybePendingKeyId() {
    if (!HasKeyId()) {
      RunCallbacksWithFailure(ServiceError::kOperationCancelled);
    }
  }

  // Returns true if a key has been assigned to this instance. Otherwise,
  // returns false which means that this instance holds a list of callbacks.
  bool HasKeyId() const {
    return std::holds_alternative<KeyIdType>(pending_callbacks_or_key_id_);
  }

  // This method should be called only if `HasKeyId()` is true.
  KeyIdType GetKeyId() const {
    CHECK(HasKeyId());
    return std::get<KeyIdType>(pending_callbacks_or_key_id_);
  }

  // These methods should be called only if `HasKeyId()` is false.

  // Adds `callback` to the list of callbacks and returns size of the list.
  size_t AddCallback(CallbackType callback) {
    CHECK(!HasKeyId());
    GetCallbacks().push_back(std::move(callback));
    return GetCallbacks().size();
  }

  void SetKeyIdAndRunCallbacks(KeyIdType key_id) {
    CHECK(!HasKeyId());
    PendingCallbacksOrKeyId pending_callbacks =
        std::exchange(pending_callbacks_or_key_id_, key_id);
    for (auto& callback : std::get<PendingCallbacks>(pending_callbacks)) {
      std::move(callback).Run(key_id);
    }
  }

  void RunCallbacksWithFailure(ServiceError error) {
    CHECK(!HasKeyId());
    for (auto& callback : std::exchange(GetCallbacks(), PendingCallbacks())) {
      std::move(callback).Run(base::unexpected(error));
    }
  }

 private:
  PendingCallbacks& GetCallbacks() {
    CHECK(!HasKeyId());
    return std::get<PendingCallbacks>(pending_callbacks_or_key_id_);
  }

  // Holds the value of its first alternative type by default.
  PendingCallbacksOrKeyId pending_callbacks_or_key_id_;
};

// A standalone task tracker representing a single in-flight key generation
// request. It wraps the raw generation task and measures the elapsed time from
// initiation to completion, reporting this duration via the completion
// callback.
template <typename KeyType>
class SpareKeyPoolRequest {
 public:
  void Start(
      crypto::UnexportableKeyProvider::Config config,
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms,
      base::RepeatingCallback<void(
          crypto::UnexportableKeyProvider::Config,
          base::span<const crypto::SignatureVerifier::SignatureAlgorithm>,
          base::OnceCallback<void(ServiceErrorOr<scoped_refptr<KeyType>>)>)>
          generate_key_fn,
      base::OnceCallback<
          void(ServiceErrorOr<scoped_refptr<KeyType>> key_or_error,
               base::TimeDelta elapsed)> completion_callback) {
    generate_key_fn.Run(
        std::move(config), acceptable_algorithms,
        base::BindOnce(&SpareKeyPoolRequest<KeyType>::OnKeyGenerated,
                       weak_factory_.GetWeakPtr(), base::TimeTicks::Now(),
                       std::move(completion_callback)));
  }

 private:
  // Measures the elapsed time for the key generation and forwards the result
  // to the completion callback.
  void OnKeyGenerated(
      base::TimeTicks start_time,
      base::OnceCallback<void(ServiceErrorOr<scoped_refptr<KeyType>>,
                              base::TimeDelta)> completion_callback,
      ServiceErrorOr<scoped_refptr<KeyType>> key_or_error) {
    std::move(completion_callback)
        .Run(std::move(key_or_error), base::TimeTicks::Now() - start_time);
  }

  base::WeakPtrFactory<SpareKeyPoolRequest<KeyType>> weak_factory_{this};
};

}  // namespace

// Templated repository class that manages maps for a single key type.
//
// Responsibilities:
// - Owns and manages the lifetime of loaded unexportable keys of type
//   `RefCountedKeyType`.
// - Maps key IDs to loaded `scoped_refptr` key objects.
// - Maps wrapped keys and tags to their resolved key IDs.
// - Deduplicates concurrent asynchronous load requests for the same wrapped
//   key.
//
// Invariants maintained:
// 1. Bidirectional Consistency: A key ID `id` is present in `key_by_key_id_`
//    if and only if its corresponding wrapped key/tag maps to `id` (or is
//    pending resolution) in `key_id_by_wrapped_key_and_tag_`.
// 2. Load Deduplication: Concurrent calls to `RegisterUnwrapKeyCallback()`
//    for the same wrapped key return the same queue, only returning `1` for
//    the first call to indicate that a new background task should be
//    scheduled.
// 3. Destruction Safety: If a `MaybePendingKeyId` or the repository is
//    destroyed, any queued callbacks are immediately cancelled and notified.
template <typename RefCountedKeyType>
class UnexportableKeyServiceImpl::KeyRepository {
 public:
  using KeyIdType = typename RefCountedKeyType::IdType;
  using KeyIdMap =
      absl::flat_hash_map<KeyIdType, scoped_refptr<RefCountedKeyType>>;

  // Retrieve key operations.
  RefCountedKeyType* GetKey(KeyIdType key_id) const {
    const auto* key = base::FindOrNull(key_by_key_id_, key_id);
    return key ? key->get() : nullptr;
  }

  // Key storage management.
  void Clear() {
    key_by_key_id_.clear();
    key_id_by_wrapped_key_and_tag_.clear();
    weak_ptr_factory_.InvalidateWeakPtrs();
  }

  // Extract key and erase from maps.
  scoped_refptr<RefCountedKeyType> ExtractKey(KeyIdType key_id) {
    auto key_handle = key_by_key_id_.extract(key_id);
    if (!key_handle) {
      return nullptr;
    }
    scoped_refptr<RefCountedKeyType> key = std::move(key_handle.mapped());
    auto wrapped_key_and_tag_handle =
        key_id_by_wrapped_key_and_tag_.extract(GetWrappedKeyAndTag(*key));
    CHECK(wrapped_key_and_tag_handle);
    auto& mapped_key_id = wrapped_key_and_tag_handle.mapped();
    CHECK(mapped_key_id.HasKeyId());
    CHECK_EQ(mapped_key_id.GetKeyId(), key_id);
    return key;
  }

  // Check if repository contains key.
  bool Contains(KeyIdType key_id) const {
    return key_by_key_id_.contains(key_id);
  }

  // Registers `callback` for the key. If the key is already loaded, runs the
  // callback immediately. Otherwise, queues the callback and returns the number
  // of callbacks currently queued for this key (returning 1 if this is the
  // first callback queued, meaning the caller must schedule a load).
  //
  // NOTE: In case the key does not exist in the map yet and we ask the backend
  // for the matching signing key, the application tag returned by the platform
  // must match the tag stored in the config. This invariant is CHECKed in
  // `OnKeyCreatedFromWrappedKey`.
  size_t RegisterUnwrapKeyCallback(
      WrappedKeyAndTagView wrapped_key_and_tag_view,
      base::OnceCallback<void(ServiceErrorOr<KeyIdType>)> callback) {
    auto& [_, maybe_pending_key_id] =
        *key_id_by_wrapped_key_and_tag_.lazy_emplace(
            wrapped_key_and_tag_view, [&](const auto& ctor) {
              ctor(Materialize(wrapped_key_and_tag_view),
                   MaybePendingKeyId<KeyIdType>());
            });

    if (maybe_pending_key_id.HasKeyId()) {
      std::move(callback).Run(maybe_pending_key_id.GetKeyId());
      return 0;
    }

    return maybe_pending_key_id.AddCallback(std::move(callback));
  }

  void OnKeyCreatedFromWrappedKey(
      WrappedKeyAndTag wrapped_key_and_tag,
      ServiceErrorOr<scoped_refptr<RefCountedKeyType>> key_or_error) {
    auto it = key_id_by_wrapped_key_and_tag_.find(wrapped_key_and_tag);
    if (it == key_id_by_wrapped_key_and_tag_.end()) {
      DVLOG(1) << "`wrapped_key` is unknown, did the key get deleted?";
      return;
    }

    MaybePendingKeyId<KeyIdType>& maybe_pending_callbacks = it->second;
    if (maybe_pending_callbacks.HasKeyId()) {
      // If there is already a key ID for this wrapped key, it means that the
      // key id has been resolved in the meantime. In this case, there is
      // nothing to do and we can return immediately.
      return;
    }

    ASSIGN_OR_RETURN(scoped_refptr<RefCountedKeyType> key,
                     std::move(key_or_error), [&](ServiceError error) {
                       auto node = key_id_by_wrapped_key_and_tag_.extract(it);
                       node.mapped().RunCallbacksWithFailure(error);
                     });
    // `key` must be non-null if `key_or_error` holds a value.
    CHECK(key);
    CHECK(wrapped_key_and_tag == GetWrappedKeyAndTag(*key));

    KeyIdType key_id = key->id();
    // A newly created key ID must be unique.
    CHECK(key_by_key_id_.try_emplace(key_id, std::move(key)).second);
    maybe_pending_callbacks.SetKeyIdAndRunCallbacks(key_id);
  }

  ServiceErrorOr<KeyIdType> OnKeyGenerated(
      ServiceErrorOr<scoped_refptr<RefCountedKeyType>> key_or_error) {
    ASSIGN_OR_RETURN(scoped_refptr<RefCountedKeyType> key,
                     std::move(key_or_error));
    // `key` must be non-null if `key_or_error` holds a value.
    CHECK(key);
    KeyIdType key_id = key->id();
    if (!key_id_by_wrapped_key_and_tag_
             .try_emplace(GetWrappedKeyAndTag(*key), key_id)
             .second) {
      // Drop a newly generated key in the case of a key collision. This should
      // be extremely rare.
      DVLOG(1) << "Collision between an existing and a newly generated key "
                  "detected.";
      return base::unexpected(ServiceError::kKeyCollision);
    }
    // A newly generated key ID must be unique.
    CHECK(key_by_key_id_.try_emplace(key_id, std::move(key)).second);
    return key_id;
  }

  base::WeakPtr<KeyRepository> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  using WrappedKeyAndTagMap =
      absl::flat_hash_map<WrappedKeyAndTag,
                          MaybePendingKeyId<KeyIdType>,
                          base::TransparentHashAs<WrappedKeyAndTagView>,
                          base::TransparentEqualAs<WrappedKeyAndTagView>>;

  WrappedKeyAndTagMap key_id_by_wrapped_key_and_tag_;
  KeyIdMap key_by_key_id_;
  base::WeakPtrFactory<KeyRepository> weak_ptr_factory_{this};
};

// Maintains a background-replenished pool of pre-generated keys grouped by
// signature algorithm to mitigate the significant latency (~1s) of
// on-demand Windows TPM key generation. Encapsulates all UMA telemetry
// logging and algorithm selection logic for the spare pool.
template <typename KeyType>
class UnexportableKeyServiceImpl::SpareKeyPool {
 public:
  SpareKeyPool(
      crypto::UnexportableKeyProvider::Config config,
      base::RepeatingCallback<void(
          crypto::UnexportableKeyProvider::Config,
          base::span<const crypto::SignatureVerifier::SignatureAlgorithm>,
          base::OnceCallback<void(ServiceErrorOr<scoped_refptr<KeyType>>)>)>
          spare_key_generation_callback)
      : config_(std::move(config)),
        spare_key_generation_callback_(
            std::move(spare_key_generation_callback)) {
    CHECK(spare_key_generation_callback_);

    // Defer the initial replenishment task to avoid competing for resources
    // during browser startup.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SpareKeyPool::ReplenishSpareKeyPoolAsync,
                       weak_ptr_factory_.GetWeakPtr(), kSpareKeyAlgorithms),
        kSpareKeyPoolDelay);
  }

  // Selects the preferred supported signature algorithm by querying the
  // provider, then attempts to pop an available pre-generated key from the
  // pool. On cache miss, calculates the total pool size and logs the specific
  // miss reason (uninitialized, failed creation, wrong algorithm, or pending
  // replenishment) to UMA.
  scoped_refptr<KeyType> PopSpareKey(
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) {
    std::unique_ptr<crypto::UnexportableKeyProvider> provider =
        UnexportableKeyTaskManager::GetUnexportableKeyProvider(config_);
    if (!provider) {
      return nullptr;
    }

    // Query the provider to select the most preferred algorithm it supports
    // from the given list, before checking the spare pool. This ensures we
    // don't inadvertently return a spare key for an algorithm that is
    // acceptable but less preferred by the provider. If it fails, it means the
    // hardware does not support any of the requested algorithms (e.g., no TPM
    // support at all).
    ASSIGN_OR_RETURN(crypto::SignatureVerifier::SignatureAlgorithm algorithm,
                     provider->SelectAlgorithm(acceptable_algorithms),
                     [this]() {
                       RecordRetrievalResult(
                           SpareKeyPoolRetrievalResult::kAlgorithmNotSupported);
                       return nullptr;
                     });

    auto* keys = base::FindOrNull(spare_keys_pool_, algorithm);

    base::UmaHistogramCounts100(
        GetSpareKeyPoolHistogramName<KeyType>("PoolSize"),
        keys ? keys->size() : 0);

    if (keys && !keys->empty()) {
      RecordRetrievalResult(SpareKeyPoolRetrievalResult::kHit);
      scoped_refptr<KeyType> spare_key = std::move(keys->back());
      keys->pop_back();
      return spare_key;
    }

    const size_t total_pool_size = GetPoolSize();

    SpareKeyPoolRetrievalResult result;
    if (total_pool_size > 0) {
      result = SpareKeyPoolRetrievalResult::kMissNoKeyForAlgorithm;
    } else if (!inflight_spare_key_pool_requests_.has_value()) {
      result = SpareKeyPoolRetrievalResult::kMissNotInitialized;
    } else if (inflight_spare_key_pool_requests_->empty()) {
      result = SpareKeyPoolRetrievalResult::kMissFailedToCreateSpareKey;
    } else {
      result = SpareKeyPoolRetrievalResult::kMissDidNotReplenishFromLastUse;
    }
    RecordRetrievalResult(result);

    return nullptr;
  }

  // Preemptively schedules background tasks to generate new spare keys until
  // the pool capacity is reached. Measures existing and pending requests to
  // avoid over-allocation. Calculates the target number of tasks upfront to
  // prevent an infinite loop if the key provider fails synchronously.
  void ReplenishSpareKeyPoolAsync(
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) {
    if (!inflight_spare_key_pool_requests_.has_value()) {
      inflight_spare_key_pool_requests_.emplace();
    }

    const size_t total_allocated =
        GetPoolSize() + inflight_spare_key_pool_requests_->size();
    if (total_allocated >= kSpareKeyPoolCapacity) {
      return;
    }

    // We must calculate the number of tasks to launch upfront instead of using
    // a `while (current_pool_size + inflight_spare_key_pool_requests_->size() <
    // capacity)` loop. If the key provider fails synchronously (e.g., due to an
    // unsupported algorithm), key generation may invoke its callback
    // synchronously. This immediately erases the request from
    // `inflight_spare_key_pool_requests_`. A `while` loop would then evaluate
    // to true forever, triggering an infinite thread lock.
    for (size_t i = 0; i < kSpareKeyPoolCapacity - total_allocated; ++i) {
      auto [it, inserted] = inflight_spare_key_pool_requests_->insert(
          std::make_unique<SpareKeyPoolRequest<KeyType>>());
      CHECK(inserted);

      (*it)->Start(config_, acceptable_algorithms,
                   spare_key_generation_callback_,
                   base::BindOnce(&SpareKeyPool::RegisterGeneratedKey,
                                  weak_ptr_factory_.GetWeakPtr(), it->get()));
    }
  }

 private:
  // The target capacity for the spare key pool. The service will attempt
  // to preemptively generate and pool keys in the background until it reaches
  // this threshold to reduce latency for future key requests.
  static constexpr size_t kSpareKeyPoolCapacity = 2;

  size_t GetPoolSize() const {
    size_t total_size = 0;
    for (const auto& [algorithm, keys] : spare_keys_pool_) {
      total_size += keys.size();
    }
    return total_size;
  }

  void RecordRetrievalResult(SpareKeyPoolRetrievalResult result) {
    base::UmaHistogramEnumeration(
        GetSpareKeyPoolHistogramName<KeyType>("RetrievalResult"), result);
  }

  // Callback invoked when an in-flight key generation request completes.
  // Registers the newly generated key, logs UMA metrics for replenishment
  // latency and generation errors, and removes the completed request from the
  // in-flight set.
  void RegisterGeneratedKey(SpareKeyPoolRequest<KeyType>* request,
                            ServiceErrorOr<scoped_refptr<KeyType>> key_or_error,
                            base::TimeDelta elapsed) {
    CHECK_EQ(inflight_spare_key_pool_requests_->erase(request), 1u);

    base::UmaHistogramEnumeration(
        GetSpareKeyPoolHistogramName<KeyType>("GenerateError"),
        key_or_error.error_or(kNoServiceErrorForMetrics));

    ASSIGN_OR_RETURN(scoped_refptr<KeyType> key, std::move(key_or_error),
                     [](ServiceError error) {});

    base::UmaHistogramMediumTimes(
        GetSpareKeyPoolHistogramName<KeyType>("ReplenishmentLatency"), elapsed);

    spare_keys_pool_[key->key().Algorithm()].push_back(std::move(key));
  }

  const crypto::UnexportableKeyProvider::Config config_;

  const base::RepeatingCallback<void(
      crypto::UnexportableKeyProvider::Config,
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>,
      base::OnceCallback<void(ServiceErrorOr<scoped_refptr<KeyType>>)>)>
      spare_key_generation_callback_;

  absl::flat_hash_map<crypto::SignatureVerifier::SignatureAlgorithm,
                      std::vector<scoped_refptr<KeyType>>>
      spare_keys_pool_;

  std::optional<
      absl::flat_hash_set<std::unique_ptr<SpareKeyPoolRequest<KeyType>>>>
      inflight_spare_key_pool_requests_;

  base::WeakPtrFactory<SpareKeyPool> weak_ptr_factory_{this};
};

UnexportableKeyServiceImpl::UnexportableKeyServiceImpl(
    UnexportableKeyTaskManager& task_manager,
    BackgroundTaskOrigin task_origin,
    crypto::UnexportableKeyProvider::Config config)
    : task_manager_(task_manager),
      task_origin_(task_origin),
      config_(config),
      signing_keys_(std::make_unique<SigningKeyRepository>()),
      attestation_keys_(std::make_unique<AttestationKeyRepository>()) {
  if (base::FeatureList::IsEnabled(kEnableUnexportableKeysSpareKeyPool)) {
    spare_signing_key_pool_ = std::make_unique<SpareSigningKeyPool>(
        config_,
        CreateGenerateKeyCallbackForSparePool<RefCountedUnexportableSigningKey>(
            &task_manager, task_origin));

    spare_attestation_key_pool_ = std::make_unique<SpareAttestationKeyPool>(
        config_,
        CreateGenerateKeyCallbackForSparePool<
            RefCountedUnexportableAttestationKey>(&task_manager, task_origin));
  }
}

UnexportableKeyServiceImpl::~UnexportableKeyServiceImpl() = default;

// static
bool UnexportableKeyServiceImpl::IsUnexportableKeyProviderSupported(
    crypto::UnexportableKeyProvider::Config config) {
  return UnexportableKeyTaskManager::GetUnexportableKeyProvider(
             std::move(config)) != nullptr;
}

// static
bool UnexportableKeyServiceImpl::IsStatefulUnexportableKeyProviderSupported(
    crypto::UnexportableKeyProvider::Config config) {
  std::unique_ptr<crypto::UnexportableKeyProvider> provider =
      UnexportableKeyTaskManager::GetUnexportableKeyProvider(std::move(config));
  return provider != nullptr &&
         provider->AsStatefulUnexportableKeyProvider() != nullptr;
}

void UnexportableKeyServiceImpl::GenerateSigningKeySlowlyAsync(
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<UnexportableSigningKeyId>)>
        callback) {
  auto wrapped_callback =
      WrapCallbackWithSpareKeyLatencyHistogram(std::move(callback));

  if (base::FeatureList::IsEnabled(kEnableUnexportableKeysSpareKeyPool)) {
    scoped_refptr<RefCountedUnexportableSigningKey> spare_key =
        spare_signing_key_pool_->PopSpareKey(acceptable_algorithms);
    spare_signing_key_pool_->ReplenishSpareKeyPoolAsync(acceptable_algorithms);
    if (spare_key) {
      std::move(wrapped_callback)
          .Run(signing_keys_->OnKeyGenerated(std::move(spare_key)));
      return;
    }
  }

  task_manager_->GenerateSigningKeySlowlyAsync(
      task_origin_, config_, acceptable_algorithms, priority,
      WrapCallbackWithErrorIfCancelled(
          std::move(wrapped_callback),
          // SAFETY: `signing_keys_` is owned by `this` and is guaranteed to be
          // alive if the projection callback is invoked (which only happens if
          // the service is still alive).
          base::BindOnce(&SigningKeyRepository::OnKeyGenerated,
                         base::Unretained(signing_keys_.get()))));
}

void UnexportableKeyServiceImpl::FromWrappedSigningKeySlowlyAsync(
    base::span<const uint8_t> wrapped_key,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<UnexportableSigningKeyId>)>
        callback) {
  WrappedKeyAndTagView key_view = {wrapped_key, GetApplicationTag(config_)};
  if (signing_keys_->RegisterUnwrapKeyCallback(key_view, std::move(callback)) ==
      1) {
    // NOTE: We don't wrap the callback in `WrapCallbackWithErrorIfCancelled`
    // here, but rather run the callbacks explicitly during the destruction of
    // `MaybePendingKeyId`.
    task_manager_->FromWrappedSigningKeySlowlyAsync(
        task_origin_, config_, wrapped_key, priority,
        base::BindOnce(&SigningKeyRepository::OnKeyCreatedFromWrappedKey,
                       signing_keys_->GetWeakPtr(), Materialize(key_view)));
  }
}

void UnexportableKeyServiceImpl::GenerateAttestationKeySlowlyAsync(
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<UnexportableAttestationKeyId>)>
        callback) {
  auto wrapped_callback =
      WrapCallbackWithSpareKeyLatencyHistogram(std::move(callback));

  if (base::FeatureList::IsEnabled(kEnableUnexportableKeysSpareKeyPool)) {
    scoped_refptr<RefCountedUnexportableAttestationKey> spare_key =
        spare_attestation_key_pool_->PopSpareKey(acceptable_algorithms);
    spare_attestation_key_pool_->ReplenishSpareKeyPoolAsync(
        acceptable_algorithms);
    if (spare_key) {
      std::move(wrapped_callback)
          .Run(attestation_keys_->OnKeyGenerated(std::move(spare_key)));
      return;
    }
  }

  task_manager_->GenerateAttestationKeySlowlyAsync(
      task_origin_, config_, acceptable_algorithms, priority,
      WrapCallbackWithErrorIfCancelled(
          std::move(wrapped_callback),
          // SAFETY: `attestation_keys_` is owned by `this` and is guaranteed to
          // be alive if the projection callback is invoked (which only happens
          // if the service is still alive).
          base::BindOnce(&AttestationKeyRepository::OnKeyGenerated,
                         base::Unretained(attestation_keys_.get()))));
}

void UnexportableKeyServiceImpl::FromWrappedAttestationKeySlowlyAsync(
    base::span<const uint8_t> wrapped_key,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<UnexportableAttestationKeyId>)>
        callback) {
  WrappedKeyAndTagView key_view = {wrapped_key, GetApplicationTag(config_)};
  if (attestation_keys_->RegisterUnwrapKeyCallback(key_view,
                                                   std::move(callback)) == 1) {
    // NOTE: We don't wrap the callback in `WrapCallbackWithErrorIfCancelled`
    // here, but rather run the callbacks explicitly during the destruction of
    // `MaybePendingKeyId`.
    task_manager_->FromWrappedAttestationKeySlowlyAsync(
        task_origin_, config_, wrapped_key, priority,
        base::BindOnce(&AttestationKeyRepository::OnKeyCreatedFromWrappedKey,
                       attestation_keys_->GetWeakPtr(), Materialize(key_view)));
  }
}

void UnexportableKeyServiceImpl::GetAllKeysForGarbageCollectionSlowlyAsync(
    BackgroundTaskPriority priority,
    base::OnceCallback<
        void(ServiceErrorOr<std::vector<UnexportableSigningKeyId>>)> callback) {
  task_manager_->GetAllKeysForGarbageCollectionSlowlyAsync(
      task_origin_, config_, priority,
      WrapCallbackWithErrorIfCancelled(
          std::move(callback),
          // SAFETY: `this` is guaranteed to be alive if the projection callback
          // is invoked.
          base::BindOnce(&UnexportableKeyServiceImpl::
                             OnGetAllKeysForGarbageCollectionSlowlyImpl,
                         base::Unretained(this))));
}

void UnexportableKeyServiceImpl::SignSlowlyAsync(
    UnexportableSigningKeyId key_id,
    base::span<const uint8_t> data,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<std::vector<uint8_t>>)> callback) {
  if (auto* key = signing_keys_->GetKey(key_id)) {
    task_manager_->SignSlowlyAsync(
        task_origin_, base::WrapRefCounted(key), data, priority,
        WrapCallbackWithErrorIfCancelled(std::move(callback)));
    return;
  }

  std::move(callback).Run(base::unexpected(ServiceError::kKeyNotFound));
}

void UnexportableKeyServiceImpl::CertifySlowlyAsync(
    UnexportableAttestationKeyId attestation_key_id,
    UnexportableSigningKeyId signing_key_id,
    base::span<const uint8_t> challenge,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<crypto::AttestationStatement>)>
        callback) {
  auto* attestation_key = attestation_keys_->GetKey(attestation_key_id);
  if (!attestation_key) {
    std::move(callback).Run(base::unexpected(ServiceError::kKeyNotFound));
    return;
  }

  auto* signing_key = signing_keys_->GetKey(signing_key_id);
  if (!signing_key) {
    std::move(callback).Run(base::unexpected(ServiceError::kKeyNotFound));
    return;
  }

  task_manager_->CertifySlowlyAsync(
      task_origin_, base::WrapRefCounted(attestation_key),
      base::WrapRefCounted(signing_key), challenge, priority,
      WrapCallbackWithErrorIfCancelled(std::move(callback)));
}

void UnexportableKeyServiceImpl::DeleteKeysSlowlyAsync(
    base::span<const UnexportableSigningKeyId> key_ids,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<size_t>)> callback) {
  // Delete the keys from the in-memory maps.
  std::vector<ServiceErrorOr<scoped_refptr<RefCountedUnexportableKey>>>
      keys_or_errors =
          base::ToVector(key_ids, [&](UnexportableSigningKeyId key_id) {
            return ExtractKeyFromMaps(key_id);
          });

  // Collect the keys that were successfully deleted.
  std::erase_if(keys_or_errors, [](auto& k) { return !k.has_value(); });
  std::vector<scoped_refptr<RefCountedUnexportableKey>> keys_to_delete =
      base::ToVector(keys_or_errors, [](auto& key) { return *std::move(key); });

  // If no keys were deleted, return an error.
  if (keys_to_delete.empty()) {
    std::move(callback).Run(base::unexpected(ServiceError::kKeyNotFound));
    return;
  }

  task_manager_->DeleteKeysSlowlyAsync(
      task_origin_, config_, std::move(keys_to_delete), priority,
      WrapCallbackWithErrorIfCancelled(std::move(callback)));
}

void UnexportableKeyServiceImpl::DeleteAllKeysSlowlyAsync(
    base::OnceCallback<void(ServiceErrorOr<size_t>)> callback) {
  signing_keys_->Clear();
  attestation_keys_->Clear();
  all_gc_keys_by_key_id_.clear();

  // Invalidate weak pointers to cancel pending key lookup requests.
  weak_ptr_factory_.InvalidateWeakPtrs();

  task_manager_->DeleteAllKeysSlowlyAsync(
      task_origin_, config_, BackgroundTaskPriority::kUserBlocking,
      WrapCallbackWithErrorIfCancelled(std::move(callback)));
}

ServiceErrorOr<std::vector<uint8_t>>
UnexportableKeyServiceImpl::GetSubjectPublicKeyInfo(
    UnexportableSigningKeyId key_id) const {
  ASSIGN_OR_RETURN(const crypto::UnexportableSigningKey* key, GetKey(key_id));
  return key->GetSubjectPublicKeyInfo();
}

ServiceErrorOr<std::vector<uint8_t>> UnexportableKeyServiceImpl::GetWrappedKey(
    UnexportableSigningKeyId key_id) const {
  ASSIGN_OR_RETURN(const crypto::UnexportableSigningKey* key, GetKey(key_id));
  return key->GetWrappedKey();
}

ServiceErrorOr<crypto::SignatureVerifier::SignatureAlgorithm>
UnexportableKeyServiceImpl::GetAlgorithm(
    UnexportableSigningKeyId key_id) const {
  ASSIGN_OR_RETURN(const crypto::UnexportableSigningKey* key, GetKey(key_id));
  return key->Algorithm();
}

ServiceErrorOr<std::string> UnexportableKeyServiceImpl::GetKeyTag(
    UnexportableSigningKeyId key_id) const {
  ASSIGN_OR_RETURN(const crypto::StatefulKey* stateful_key,
                   GetStatefulKey(key_id));
  return stateful_key->GetKeyTag();
}

ServiceErrorOr<base::Time> UnexportableKeyServiceImpl::GetCreationTime(
    UnexportableSigningKeyId key_id) const {
  ASSIGN_OR_RETURN(const crypto::StatefulKey* stateful_key,
                   GetStatefulKey(key_id));
  return stateful_key->GetCreationTime();
}

ServiceErrorOr<const crypto::UnexportableSigningKey*>
UnexportableKeyServiceImpl::GetKey(UnexportableSigningKeyId key_id) const {
  if (auto* key = signing_keys_->GetKey(key_id)) {
    return &key->key();
  }
  if (auto* key =
          attestation_keys_->GetKey(UnexportableAttestationKeyId(key_id))) {
    return &key->key();
  }
  if (const auto* key = base::FindOrNull(all_gc_keys_by_key_id_, key_id)) {
    return &(*key)->key();
  }
  return base::unexpected(ServiceError::kKeyNotFound);
}

ServiceErrorOr<const crypto::StatefulKey*>
UnexportableKeyServiceImpl::GetStatefulKey(
    UnexportableSigningKeyId key_id) const {
  ASSIGN_OR_RETURN(const crypto::UnexportableSigningKey* key, GetKey(key_id));
  if (const crypto::StatefulKey* stateful_key = key->AsStatefulKey()) {
    return stateful_key;
  }
  return base::unexpected(ServiceError::kOperationNotSupported);
}

ServiceErrorOr<scoped_refptr<RefCountedUnexportableKey>>
UnexportableKeyServiceImpl::ExtractKeyFromMaps(
    UnexportableSigningKeyId key_id) {
  // Check the garbage collection map first. Ensure the `key_id` can't be
  // present in the other maps.
  if (auto gc_key_handle = all_gc_keys_by_key_id_.extract(key_id)) {
    CHECK(!signing_keys_->Contains(key_id));
    CHECK(!attestation_keys_->Contains(UnexportableAttestationKeyId(key_id)));
    return std::move(gc_key_handle.mapped());
  }

  if (auto key = signing_keys_->ExtractKey(key_id)) {
    return key;
  }

  if (auto key =
          attestation_keys_->ExtractKey(UnexportableAttestationKeyId(key_id))) {
    return key;
  }

  return base::unexpected(ServiceError::kKeyNotFound);
}

ServiceErrorOr<std::vector<UnexportableSigningKeyId>>
UnexportableKeyServiceImpl::OnGetAllKeysForGarbageCollectionSlowlyImpl(
    ServiceErrorOr<std::vector<scoped_refptr<RefCountedUnexportableKey>>>
        keys_or_error) {
  ASSIGN_OR_RETURN(std::vector<scoped_refptr<RefCountedUnexportableKey>> keys,
                   std::move(keys_or_error));

  auto key_ids = base::ToVector(keys, [](auto& key) { return key->id(); });
  all_gc_keys_by_key_id_.clear();
  all_gc_keys_by_key_id_.reserve(keys.size());
  for (auto& key : keys) {
    all_gc_keys_by_key_id_.emplace(key->id(), std::move(key));
  }
  return key_ids;
}

}  // namespace unexportable_keys
