// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/extensions_activity.h"

namespace syncer {

ExtensionsActivity::ExtensionsActivity() = default;

ExtensionsActivity::~ExtensionsActivity() = default;

void ExtensionsActivity::GetAndClearRecords(Records* buffer) {
  base::AutoLock lock(records_lock_);
  buffer->clear();
  buffer->swap(records_);
}

void ExtensionsActivity::PutRecords(const Records& records) {
  base::AutoLock lock(records_lock_);
  for (const auto& [id, record] : records) {
    records_[id].extension_id = record.extension_id;
    records_[id].bookmark_write_count += record.bookmark_write_count;
  }
}

void ExtensionsActivity::UpdateRecord(const std::string& extension_id) {
  base::AutoLock lock(records_lock_);
  Record& record = records_[extension_id];
  record.extension_id = extension_id;
  record.bookmark_write_count++;
}

}  // namespace syncer
