// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/lock.h"

#include <memory>
#include <ostream>

#include "chrome/browser/web_applications/locks/partitioned_lock_manager.h"
#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"
#include "chrome/browser/web_applications/visited_manifest_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/webapps/common/web_app_id.h"

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

LockDescription::LockDescription(base::flat_set<webapps::AppId> app_ids,
                                 LockDescription::Type type)
    : app_ids_(std::move(app_ids)), type_(type) {
  for (const webapps::AppId& app_id : app_ids_) {
    CHECK(!app_id.empty()) << "Cannot have an empty app_id";
  }
}
LockDescription::LockDescription(LockDescription&&) = default;

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

WebContentsManager& Lock::web_contents_manager() {
  CHECK(lock_manager_);
  return lock_manager_->provider().web_contents_manager();
}

VisitedManifestManager& Lock::visited_manifest_manager() {
  CHECK(lock_manager_);
  return lock_manager_->provider().visited_manifest_manager();
}

Lock::Lock() : holder_(std::make_unique<PartitionedLockHolder>()) {}
Lock::~Lock() = default;

bool Lock::IsGranted() const {
  return !!lock_manager_;
}

void Lock::GrantLockResources(WebAppLockManager& lock_manager) {
  CHECK(!lock_manager_);
  lock_manager_ = lock_manager.GetWeakPtr();
}

}  // namespace web_app
