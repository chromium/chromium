// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_origin_association_task.h"

#include <utility>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "components/webapps/services/web_app_origin_association/public/mojom/web_app_origin_association_parser.mojom.h"
#include "components/webapps/services/web_app_origin_association/web_app_origin_association_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace web_app {

WebAppOriginAssociationManager::Task::Task(
    const GURL& web_app_identity,
    ScopeExtensions scope_extensions,
    WebAppOriginAssociationManager& manager,
    OnDidGetWebAppOriginAssociations callback)
    : web_app_identity_(web_app_identity),
      owner_(manager),
      callback_(std::move(callback)) {
  for (auto& scope_extension : scope_extensions) {
    pending_scope_extensions_.push_back(std::move(scope_extension));
  }
  scope_extensions.clear();
}

WebAppOriginAssociationManager::Task::~Task() = default;

void WebAppOriginAssociationManager::Task::Start() {
  if (pending_scope_extensions_.empty()) {
    Finalize();
    return;
  }

  FetchAssociationFile(GetCurrentScopeExtension());
}

ScopeExtensionInfo&
WebAppOriginAssociationManager::Task::GetCurrentScopeExtension() {
  DCHECK(!pending_scope_extensions_.empty());
  return pending_scope_extensions_.front();
}

void WebAppOriginAssociationManager::Task::FetchAssociationFile(
    ScopeExtensionInfo& scope_extension) {
  if (scope_extension.origin.Serialize().empty()) {
    MaybeStartNextScopeExtension();
    return;
  }

  owner_->GetFetcher().FetchWebAppOriginAssociationFile(
      scope_extension.origin, g_browser_process->shared_url_loader_factory(),
      base::BindOnce(
          &WebAppOriginAssociationManager::Task::OnAssociationFileFetched,
          weak_ptr_factory_.GetWeakPtr()));
}

void WebAppOriginAssociationManager::Task::OnAssociationFileFetched(
    std::unique_ptr<std::string> file_content) {
  if (!file_content || file_content->empty()) {
    MaybeStartNextScopeExtension();
    return;
  }

  owner_->GetParser()->ParseWebAppOriginAssociation(
      *file_content,
      base::BindOnce(&WebAppOriginAssociationManager::Task::OnAssociationParsed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebAppOriginAssociationManager::Task::OnAssociationParsed(
    webapps::mojom::WebAppOriginAssociationPtr association,
    std::vector<webapps::mojom::WebAppOriginAssociationErrorPtr> errors) {
  if (association.is_null() || association->apps.empty()) {
    MaybeStartNextScopeExtension();
    return;
  }

  auto& scope_extension = GetCurrentScopeExtension();
  for (auto& associated_app : association->apps) {
    if (associated_app->web_app_identity == web_app_identity_) {
      result_.insert(scope_extension);
      scope_extension.Reset();
      // Only information in the first valid app is saved.
      break;
    }
  }
  association->apps.clear();

  MaybeStartNextScopeExtension();
}

void WebAppOriginAssociationManager::Task::MaybeStartNextScopeExtension() {
  DCHECK(!pending_scope_extensions_.empty());
  pending_scope_extensions_.pop_front();
  if (!pending_scope_extensions_.empty()) {
    // Continue with the next scope extension.
    FetchAssociationFile(GetCurrentScopeExtension());
    return;
  }

  Finalize();
}

void WebAppOriginAssociationManager::Task::Finalize() {
  ScopeExtensions result = std::move(result_);
  result_.clear();
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), std::move(result)));
  owner_->OnTaskCompleted();
}

}  // namespace web_app
