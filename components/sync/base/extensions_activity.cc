// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/extensions_activity.h"

namespace syncer {

ExtensionsActivity::Record::Record() : bookmark_write_count(0U) {}

ExtensionsActivity::Record::~Record() {}

ExtensionsActivity::ExtensionsActivity() {}

ExtensionsActivity::~ExtensionsActivity() {}

void ExtensionsActivity::GetAndClearRecords(Records* buffer) {
  base::AutoLock lock(records_lock_);
  buffer->clear();
  buffer->swap(records_);
}

void ExtensionsActivity::PutRecords(const Records& records) {
  base::AutoLock lock(records_lock_);
  for (auto i = records.begin(); i != records.end(); ++i) {
    records_[i->first].extension_id = i->second.extension_id;
    records_[i->first].bookmark_write_count += i->second.bookmark_write_count;
  }
}

void ExtensionsActivity::UpdateRecord(const std::string& extension_id) {
  base::AutoLock lock(records_lock_);
  Record& record = records_[extension_id];
  record.extension_id = extension_id;
  record.bookmark_write_count++;
}

}  // namespace syncer
