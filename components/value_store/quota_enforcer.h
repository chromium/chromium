// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VALUE_STORE_QUOTA_ENFORCER_H_
#define COMPONENTS_VALUE_STORE_QUOTA_ENFORCER_H_

#include <stddef.h>

#include <limits>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/stack_allocated.h"
#include "components/value_store/value_store.h"

namespace value_store {

// Reuses JSON serialized during Set() quota staging when writing to LevelDB
// and computes initial quota usage directly from the backing store.
BASE_DECLARE_FEATURE(kOptimizeValueStoreQuotaEnforcement);

// Inheritable quota-enforcement base class for ValueStore implementations.
class QuotaEnforcer : public ValueStore {
 public:
  struct Limits {
    size_t quota_bytes = std::numeric_limits<size_t>::max();
    size_t quota_bytes_per_item = std::numeric_limits<size_t>::max();
    size_t max_items = std::numeric_limits<size_t>::max();
  };

  QuotaEnforcer(const QuotaEnforcer&) = delete;
  QuotaEnforcer& operator=(const QuotaEnforcer&) = delete;

  ~QuotaEnforcer() override;

  void SetQuotaLimits(const Limits& limits);

  // ValueStore:
  size_t GetBytesInUse(const std::string& key) override;
  size_t GetBytesInUse(const std::vector<std::string>& keys) override;
  size_t GetBytesInUse() override;

 protected:
  // Stack-scoped transaction that stages Set() quota changes via Stage(),
  // committing via Commit() or discarding on early exit.
  class ScopedQuotaTransaction {
    STACK_ALLOCATED();

   public:
    explicit ScopedQuotaTransaction(QuotaEnforcer* enforcer);
    ~ScopedQuotaTransaction();

    ScopedQuotaTransaction(const ScopedQuotaTransaction&) = delete;
    ScopedQuotaTransaction& operator=(const ScopedQuotaTransaction&) = delete;

    // Stages and checks quota for a single-key or multi-key Set() (unless
    // `IGNORE_QUOTA`). Populates `value_as_json` or `serialized_values` with
    // the serialized JSON when quota calculation is needed, the output
    // parameter is non-null, and `kOptimizeValueStoreQuotaEnforcement` is
    // enabled, so the backing store can reuse it without re-serializing.
    Status Stage(WriteOptions options,
                 const std::string& key,
                 const base::Value& value,
                 std::optional<std::string>* value_as_json = nullptr);
    Status Stage(WriteOptions options,
                 const base::DictValue& settings,
                 std::vector<std::string>* serialized_values = nullptr);

    void Commit();

   private:
    Status StageKey(WriteOptions options,
                    const std::string& key,
                    const base::Value& value,
                    std::optional<std::string>* value_as_json);
    Status Check(WriteOptions options) const;

    QuotaEnforcer* const enforcer_;
    size_t staged_used_total_ = 0;
    std::map<std::string, size_t> staged_used_per_setting_;
    bool committed_ = false;
  };

  QuotaEnforcer();

  // Populates initial byte usage via RecordStoredItemSize().
  virtual bool CalculateUsage() = 0;

  void RecordStoredItemSize(std::string key, size_t item_byte_size);
  void FreeQuotaUsage(const std::string& key);
  void ResetQuotaUsage();

 private:
  enum class QuotaResource {
    kQuotaBytes,
    kQuotaBytesPerItem,
    kMaxItems,
  };

  static void Allocate(const std::string& key,
                       size_t value_byte_size,
                       size_t* used_total,
                       std::map<std::string, size_t>* used_per_setting);
  static Status QuotaExceededError(QuotaResource resource);

  bool NeedsQuotaCalculation() const {
    return limits_.has_value() || usage_calculated_;
  }
  void LazyCalculateUsage();

  std::optional<Limits> limits_;
  size_t used_total_ = 0;
  bool usage_calculated_ = false;
  std::map<std::string, size_t> used_per_setting_;
};

}  // namespace value_store

#endif  // COMPONENTS_VALUE_STORE_QUOTA_ENFORCER_H_
