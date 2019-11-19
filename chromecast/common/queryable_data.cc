// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/queryable_data.h"

#include <utility>

namespace chromecast {

namespace {
QueryableData& GetQueryableData() {
  static base::NoDestructor<QueryableData> queryable_data;
  return *queryable_data;
}
}  // namespace

// static
void QueryableData::RegisterQueryableValue(const std::string& query_key,
                                           base::Value initial_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(GetQueryableData().sequence_checker_);
  GetQueryableData().queryable_values_[query_key] = std::move(initial_value);
}

// static
const base::Value* QueryableData::Query(const std::string& query_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(GetQueryableData().sequence_checker_);
  const QueryableData& data = GetQueryableData();

  auto value = data.queryable_values_.find(query_key);
  if (value == data.queryable_values_.end())
    return nullptr;
  return &value->second;
}

// static
const QueryableData::ValueMap& QueryableData::GetValues() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(GetQueryableData().sequence_checker_);
  const QueryableData& data = GetQueryableData();
  return data.queryable_values_;
}

QueryableData::QueryableData() {}

QueryableData::~QueryableData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace chromecast
