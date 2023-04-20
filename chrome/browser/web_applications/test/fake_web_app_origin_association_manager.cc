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
    ScopeExtensions scope_extensions,
    OnDidGetWebAppOriginAssociations callback) {
  ScopeExtensions result;

  if (pass_through_) {
    result = scope_extensions;
  } else {
    for (const auto& scope_extension : scope_extensions) {
      auto it = data_.find(scope_extension);
      if (it != data_.end())
        result.insert(it->second);
    }
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void FakeWebAppOriginAssociationManager::SetData(
    std::map<ScopeExtensionInfo, ScopeExtensionInfo> data) {
  data_ = std::move(data);
}

}  // namespace web_app
