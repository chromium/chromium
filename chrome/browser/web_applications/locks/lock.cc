// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/lock.h"

#include <memory>
#include <ostream>

#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"

namespace web_app {

std::string LockTypeToString(LockDescription::Type type) {
  switch (type) {
    case web_app::LockDescription::Type::kApp:
      return "App";
    case web_app::LockDescription::Type::kAppAndWebContents:
      return "AppAndWebContents";
    case web_app::LockDescription::Type::kBackgroundWebContents:
      return "WebContents";
    case web_app::LockDescription::Type::kAllAppsLock:
      return "AllApps";
    case web_app::LockDescription::Type::kNoOp:
      return "NoOp";
  }
}

LockDescription::LockDescription(base::flat_set<AppId> app_ids,
                                 LockDescription::Type type)
    : app_ids_(std::move(app_ids)), type_(type) {}
LockDescription::~LockDescription() = default;

bool LockDescription::IncludesSharedWebContents() const {
  switch (type_) {
    case Type::kNoOp:
    case Type::kApp:
    case Type::kAllAppsLock:
      return false;
    case Type::kBackgroundWebContents:
    case Type::kAppAndWebContents:
      return true;
  }
}
base::Value LockDescription::AsDebugValue() const {
  base::Value::Dict result;
  base::Value::List ids;
  ids.reserve(app_ids_.size());
  for (const auto& id : app_ids_) {
    ids.Append(id);
  }
  result.Set("type", LockTypeToString(type()));
  result.Set("app_ids", std::move(ids));
  return base::Value(std::move(result));
}

std::ostream& operator<<(std::ostream& out,
                         const LockDescription& lock_description) {
  return out << lock_description.AsDebugValue();
}

Lock::Lock(std::unique_ptr<content::PartitionedLockHolder> holder)
    : holder_(std::move(holder)) {}

Lock::~Lock() = default;

}  // namespace web_app
