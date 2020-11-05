// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/federated_learning/floc_id.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/federated_learning/floc_constants.h"
#include "components/federated_learning/sim_hash.h"

namespace federated_learning {

namespace {

// Domain one-hot + sim hash + sorting-lsh (behind a feature flag)
const uint32_t kChromeFlocIdVersion = 1;

}  // namespace

// static
uint64_t FlocId::SimHashHistory(
    const std::unordered_set<std::string>& domains) {
  return SimHashStrings(domains, kMaxNumberOfBitsInFloc);
}

FlocId::FlocId() = default;

FlocId::FlocId(uint64_t id, uint32_t sorting_lsh_version)
    : id_(id), sorting_lsh_version_(sorting_lsh_version) {}

FlocId::FlocId(const FlocId& id) = default;

FlocId::~FlocId() = default;

FlocId& FlocId::operator=(const FlocId& id) = default;

FlocId& FlocId::operator=(FlocId&& id) = default;

bool FlocId::IsValid() const {
  return id_.has_value();
}

bool FlocId::operator==(const FlocId& other) const {
  return id_ == other.id_ && sorting_lsh_version_ == other.sorting_lsh_version_;
}

bool FlocId::operator!=(const FlocId& other) const {
  return !(*this == other);
}

uint64_t FlocId::ToUint64() const {
  DCHECK(id_.has_value());
  return id_.value();
}

std::string FlocId::ToString() const {
  DCHECK(id_.has_value());

  return base::StrCat({base::NumberToString(id_.value()), ".",
                       base::NumberToString(kChromeFlocIdVersion), ".",
                       base::NumberToString(sorting_lsh_version_)});
}

}  // namespace federated_learning
