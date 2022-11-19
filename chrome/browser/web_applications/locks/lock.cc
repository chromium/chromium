// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/lock.h"

#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"

namespace web_app {

LockDescription::LockDescription(base::flat_set<AppId> app_ids,
                                 LockDescription::Type type)
    : app_ids_(std::move(app_ids)), type_(type) {}
LockDescription::~LockDescription() = default;

bool LockDescription::IncludesSharedWebContents() const {
  switch (type_) {
    case Type::kNoOp:
    case Type::kFullSystem:
    case Type::kApp:
      return false;
    case Type::kBackgroundWebContents:
    case Type::kAppAndWebContents:
      return true;
  }
}

Lock::Lock(std::unique_ptr<content::PartitionedLockHolder> holder)
    : holder_(std::move(holder)) {}

Lock::~Lock() = default;

}  // namespace web_app
