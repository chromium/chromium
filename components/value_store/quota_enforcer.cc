// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/value_store/quota_enforcer.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"

namespace value_store {

namespace {

const char kCannotSerialize[] = "Cannot serialize value to JSON";

}  // namespace

BASE_FEATURE(kOptimizeValueStoreQuotaEnforcement,
             base::FEATURE_DISABLED_BY_DEFAULT);

QuotaEnforcer::ScopedQuotaTransaction::ScopedQuotaTransaction(
    QuotaEnforcer* enforcer)
    : enforcer_(enforcer) {
  CHECK(enforcer_);
  if (!enforcer_->NeedsQuotaCalculation()) {
    return;
  }
  enforcer_->LazyCalculateUsage();
  staged_used_total_ = enforcer_->used_total_;
  staged_used_per_setting_ = enforcer_->used_per_setting_;
}

QuotaEnforcer::ScopedQuotaTransaction::~ScopedQuotaTransaction() = default;

ValueStore::Status QuotaEnforcer::ScopedQuotaTransaction::Stage(
    WriteOptions options,
    const std::string& key,
    const base::Value& value,
    std::optional<std::string>* value_as_json) {
  CHECK(!committed_);
  if (!enforcer_->NeedsQuotaCalculation()) {
    return Status();
  }
  if (Status status = StageKey(options, key, value, value_as_json);
      !status.ok()) {
    return status;
  }
  return Check(options);
}

ValueStore::Status QuotaEnforcer::ScopedQuotaTransaction::Stage(
    WriteOptions options,
    const base::DictValue& settings,
    std::vector<std::string>* serialized_values) {
  CHECK(!committed_);
  if (!enforcer_->NeedsQuotaCalculation()) {
    return Status();
  }
  const bool retain_json =
      serialized_values &&
      base::FeatureList::IsEnabled(kOptimizeValueStoreQuotaEnforcement);
  if (retain_json) {
    serialized_values->reserve(settings.size());
  }
  for (const auto [key, value] : settings) {
    std::optional<std::string> value_as_json;
    if (Status status = StageKey(options, key, value,
                                 retain_json ? &value_as_json : nullptr);
        !status.ok()) {
      return status;
    }
    if (retain_json) {
      serialized_values->push_back(std::move(*value_as_json));
    }
  }
  return Check(options);
}

void QuotaEnforcer::ScopedQuotaTransaction::Commit() {
  CHECK(!committed_);
  committed_ = true;
  if (enforcer_->usage_calculated_) {
    enforcer_->used_total_ = staged_used_total_;
    enforcer_->used_per_setting_ = std::move(staged_used_per_setting_);
  }
}

ValueStore::Status QuotaEnforcer::ScopedQuotaTransaction::StageKey(
    WriteOptions options,
    const std::string& key,
    const base::Value& value,
    std::optional<std::string>* value_as_json) {
  if (!enforcer_->NeedsQuotaCalculation()) {
    return Status();
  }
  const bool retain_json =
      value_as_json &&
      base::FeatureList::IsEnabled(kOptimizeValueStoreQuotaEnforcement);
  std::optional<std::string> serialized = base::WriteJson(value);
  if (!serialized) {
    return Status(OTHER_ERROR, kCannotSerialize);
  }
  Allocate(key, serialized->size(), &staged_used_total_,
           &staged_used_per_setting_);
  if (enforcer_->limits_ && !(options & IGNORE_QUOTA) &&
      staged_used_per_setting_[key] >
          enforcer_->limits_->quota_bytes_per_item) {
    return QuotaExceededError(QuotaResource::kQuotaBytesPerItem);
  }
  if (retain_json) {
    *value_as_json = std::move(serialized);
  }
  return Status();
}

ValueStore::Status QuotaEnforcer::ScopedQuotaTransaction::Check(
    WriteOptions options) const {
  if (enforcer_->limits_ && !(options & IGNORE_QUOTA)) {
    if (staged_used_total_ > enforcer_->limits_->quota_bytes) {
      return QuotaExceededError(QuotaResource::kQuotaBytes);
    }
    if (staged_used_per_setting_.size() > enforcer_->limits_->max_items) {
      return QuotaExceededError(QuotaResource::kMaxItems);
    }
  }
  return Status();
}

QuotaEnforcer::QuotaEnforcer() = default;
QuotaEnforcer::~QuotaEnforcer() = default;

void QuotaEnforcer::SetQuotaLimits(const Limits& limits) {
  limits_ = limits;
  ResetQuotaUsage();
}

size_t QuotaEnforcer::GetBytesInUse(const std::string& key) {
  LazyCalculateUsage();
  auto it = used_per_setting_.find(key);
  return it == used_per_setting_.end() ? 0u : it->second;
}

size_t QuotaEnforcer::GetBytesInUse(const std::vector<std::string>& keys) {
  size_t used = 0;
  for (const std::string& key : keys) {
    used += GetBytesInUse(key);
  }
  return used;
}

size_t QuotaEnforcer::GetBytesInUse() {
  LazyCalculateUsage();
  return used_total_;
}

void QuotaEnforcer::RecordStoredItemSize(std::string key,
                                         size_t item_byte_size) {
  used_total_ += item_byte_size;
  used_per_setting_[std::move(key)] = item_byte_size;
}

void QuotaEnforcer::FreeQuotaUsage(const std::string& key) {
  if (!usage_calculated_) {
    return;
  }
  auto it = used_per_setting_.find(key);
  if (it == used_per_setting_.end()) {
    return;
  }
  DCHECK_GE(used_total_, it->second);
  used_total_ -= it->second;
  used_per_setting_.erase(it);
}

void QuotaEnforcer::ResetQuotaUsage() {
  used_per_setting_.clear();
  used_total_ = 0u;
  usage_calculated_ = false;
}

// static
void QuotaEnforcer::Allocate(const std::string& key,
                             size_t value_byte_size,
                             size_t* used_total,
                             std::map<std::string, size_t>* used_per_setting) {
  size_t new_size = key.size() + value_byte_size;
  size_t existing_size = (*used_per_setting)[key];

  *used_total += (new_size - existing_size);
  (*used_per_setting)[key] = new_size;
}

// static
ValueStore::Status QuotaEnforcer::QuotaExceededError(QuotaResource resource) {
  const char* name = nullptr;
  switch (resource) {
    case QuotaResource::kQuotaBytes:
      name = "Resource::kQuotaBytes";
      break;
    case QuotaResource::kQuotaBytesPerItem:
      name = "Resource::kQuotaBytesPerItem";
      break;
    case QuotaResource::kMaxItems:
      name = "Resource::kMaxItems";
      break;
  }
  CHECK(name);
  return Status(QUOTA_EXCEEDED, base::StringPrintf("%s quota exceeded", name));
}

void QuotaEnforcer::LazyCalculateUsage() {
  if (usage_calculated_) {
    return;
  }

  DCHECK_EQ(0u, used_total_);
  DCHECK(used_per_setting_.empty());

  if (base::FeatureList::IsEnabled(kOptimizeValueStoreQuotaEnforcement)) {
    if (!CalculateUsage()) {
      ResetQuotaUsage();
      return;
    }
  } else {
    ReadResult maybe_settings = Get();
    if (!maybe_settings.status().ok()) {
      LOG(WARNING) << "Failed to get settings for quota: "
                   << maybe_settings.status().message;
      ResetQuotaUsage();
      return;
    }

    for (const auto [key, value] : maybe_settings.settings()) {
      std::string value_as_json = base::WriteJson(value).value_or("");
      RecordStoredItemSize(key, key.size() + value_as_json.size());
    }
  }

  usage_calculated_ = true;
}

}  // namespace value_store
