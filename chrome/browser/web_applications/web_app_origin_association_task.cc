// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_origin_association_task.h"

#include <optional>
#include <set>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "components/webapps/services/web_app_origin_association/web_app_origin_association_fetcher.h"
#include "components/webapps/services/web_app_origin_association/web_app_origin_association_parser.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

WebAppOriginAssociationManager::Task::Task(
    const GURL& web_app_identity,
    OriginAssociations origin_associations,
    WebAppOriginAssociationManager& manager,
    OnDidGetWebAppOriginAssociations callback)
    : web_app_identity_(web_app_identity),
      scope_extensions_input_(std::move(origin_associations.scope_extensions)),
      migration_sources_input_(
          std::move(origin_associations.migration_sources)),
      owner_(manager),
      callback_(std::move(callback)) {
  std::set<url::Origin> unique_origins;
  for (const ScopeExtensionInfo& scope_extension : scope_extensions_input_) {
    if (!scope_extension.origin.opaque()) {
      unique_origins.insert(scope_extension.origin);
    }
  }
  for (const proto::WebAppMigrationSource& migration_source :
       migration_sources_input_) {
    url::Origin origin =
        url::Origin::Create(GURL(migration_source.manifest_id()));
    CHECK(!origin.opaque());
    // Same origin migration is allowed without checking a .well-known file, so
    // no need to do origin validation for that source.
    if (origin.IsSameOriginWith(web_app_identity_)) {
      continue;
    }
    unique_origins.insert(origin);
  }
  pending_origins_.assign(unique_origins.begin(), unique_origins.end());
}

WebAppOriginAssociationManager::Task::~Task() = default;

void WebAppOriginAssociationManager::Task::Start() {
  MaybeStartNextStep();
}

void WebAppOriginAssociationManager::Task::FetchAssociationFile(
    const url::Origin& origin) {
  owner_->GetFetcher().FetchWebAppOriginAssociationFile(
      origin, g_browser_process->shared_url_loader_factory(),
      base::BindOnce(
          &WebAppOriginAssociationManager::Task::OnAssociationFileFetched,
          weak_ptr_factory_.GetWeakPtr(), origin));
}

void WebAppOriginAssociationManager::Task::OnAssociationFileFetched(
    const url::Origin& origin,
    std::optional<std::string> file_content) {
  if (!file_content || file_content->empty()) {
    MaybeStartNextStep();
    return;
  }

  base::expected<webapps::ParsedAssociations, std::string> parse_result =
      webapps::ParseWebAppOriginAssociations(*file_content, origin);

  if (parse_result.has_value()) {
    for (webapps::AssociatedWebApp& app : parse_result->apps) {
      if (app.web_app_identity == web_app_identity_) {
        fetched_associations_[origin] = std::move(app);
        // Only information in the first valid app is saved.
        break;
      }
    }
  }

  MaybeStartNextStep();
}

void WebAppOriginAssociationManager::Task::MaybeStartNextStep() {
  if (pending_origins_.empty()) {
    Finalize();
    return;
  }

  url::Origin origin = pending_origins_.front();
  pending_origins_.pop_front();
  FetchAssociationFile(origin);
}

void WebAppOriginAssociationManager::Task::Finalize() {
  for (ScopeExtensionInfo scope_extension : scope_extensions_input_) {
    auto it = fetched_associations_.find(scope_extension.origin);
    if (it == fetched_associations_.end()) {
      continue;
    }

    const auto& associated_app = it->second;
    // Must drop the fragments and queries per `scope` rules
    // https://w3c.github.io/manifest/#scope-member
    GURL::Replacements replacements;
    replacements.ClearRef();
    replacements.ClearQuery();
    scope_extension.scope =
        associated_app.scope.ReplaceComponents(replacements);
    result_.scope_extensions.insert(scope_extension);
  }

  for (const proto::WebAppMigrationSource& migration_source :
       migration_sources_input_) {
    url::Origin origin_to_check =
        url::Origin::Create(GURL(migration_source.manifest_id()));
    if (origin_to_check.opaque()) {
      continue;
    }

    if (origin_to_check.IsSameOriginWith(web_app_identity_)) {
      result_.migration_sources.push_back(migration_source);
      continue;
    }

    auto it = fetched_associations_.find(origin_to_check);
    if (it == fetched_associations_.end()) {
      continue;
    }

    if (it->second.allow_migration) {
      result_.migration_sources.push_back(migration_source);
    }
  }

  OriginAssociations result = std::move(result_);
  result_ = OriginAssociations();
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), std::move(result)));
  owner_->OnTaskCompleted();
}

}  // namespace web_app
