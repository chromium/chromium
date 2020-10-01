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

constexpr char kFlocVersion[] = "1.0.0";

}  // namespace

// static
FlocId FlocId::CreateFromHistory(
    const std::unordered_set<std::string>& domains) {
  return FlocId(SimHashStrings(domains, kMaxNumberOfBitsInFloc));
}

FlocId::FlocId() = default;

FlocId::FlocId(uint64_t id) : id_(id) {}

FlocId::FlocId(const FlocId& id) = default;

FlocId::~FlocId() = default;

FlocId& FlocId::operator=(const FlocId& id) = default;

FlocId& FlocId::operator=(FlocId&& id) = default;

bool FlocId::IsValid() const {
  return id_.has_value();
}

bool FlocId::operator==(const FlocId& other) const {
  return id_ == other.id_;
}

bool FlocId::operator!=(const FlocId& other) const {
  return !(*this == other);
}

uint64_t FlocId::ToUint64() const {
  DCHECK(id_.has_value());
  return id_.value();
}

std::string FlocId::ToDebugHeaderValue() const {
  if (!id_.has_value())
    return "null";
  return ToString();
}

std::string FlocId::ToString() const {
  DCHECK(id_.has_value());
  return base::StrCat({base::NumberToString(id_.value()), ".", kFlocVersion});
}

}  // namespace federated_learning
