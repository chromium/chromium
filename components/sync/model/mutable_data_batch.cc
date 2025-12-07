// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/mutable_data_batch.h"

#include "base/check.h"
#include "components/sync/protocol/entity_data.h"

namespace syncer {

MutableDataBatch::MutableDataBatch() = default;

MutableDataBatch::~MutableDataBatch() = default;

void MutableDataBatch::Put(const std::string& storage_key,
                           std::unique_ptr<EntityData> specifics) {
  key_data_pairs_.emplace_back(storage_key, std::move(specifics));
}

bool MutableDataBatch::HasNext() const {
  return key_data_pairs_.size() > read_index_;
}

KeyAndData MutableDataBatch::Next() {
  DCHECK(HasNext());
  return std::move(key_data_pairs_[read_index_++]);
}

}  // namespace syncer
