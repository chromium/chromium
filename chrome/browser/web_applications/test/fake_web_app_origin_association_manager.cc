// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/fake_web_app_origin_association_manager.h"

#include <utility>

#include "base/task/sequenced_task_runner.h"

namespace web_app {

FakeWebAppOriginAssociationManager::FakeWebAppOriginAssociationManager() =
    default;

FakeWebAppOriginAssociationManager::~FakeWebAppOriginAssociationManager() =
    default;

void FakeWebAppOriginAssociationManager::GetWebAppOriginAssociations(
    const GURL& web_app_identity,
    OriginAssociations origin_associations,
    OnDidGetWebAppOriginAssociations callback) {
  OriginAssociations result;

  if (pass_through_) {
    result = origin_associations;
  } else {
    for (const auto& scope_extension : origin_associations.scope_extensions) {
      auto it = data_.find(scope_extension);
      if (it != data_.end()) {
        result.scope_extensions.insert(it->second);
      }
    }
    for (const auto& migration_source : origin_associations.migration_sources) {
      if (migration_sources_data_.contains(
              webapps::ManifestId(migration_source.manifest_id()))) {
        result.migration_sources.push_back(migration_source);
      }
    }
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void FakeWebAppOriginAssociationManager::SetData(
    std::map<ScopeExtensionInfo, ScopeExtensionInfo> data) {
  data_ = std::move(data);
}

void FakeWebAppOriginAssociationManager::SetMigrationSourcesData(
    base::flat_set<webapps::ManifestId> data) {
  migration_sources_data_ = std::move(data);
}

}  // namespace web_app
