// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_origin_association_task.h"

#include <utility>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "components/webapps/services/web_app_origin_association/public/mojom/web_app_origin_association_parser.mojom.h"
#include "components/webapps/services/web_app_origin_association/web_app_origin_association_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace {
// Keep in sync with
// third_party/blink/renderer/modules/manifest/manifest_parser.cc.
constexpr size_t kMaxPathsSize = 10;
constexpr size_t kMaxPathLength = 2000;

// Number of paths cannot exceed |kMaxPathsSize|, and each path cannot contain
// more than |kMaxPathLength| characters. Duplicate paths are ignored and do not
// count towards kMaxPathsSize.
std::vector<std::string> GetValidPaths(std::vector<std::string> paths) {
  base::flat_set<std::string> result;
  for (std::string& path : paths) {
    if (result.size() == kMaxPathsSize)
      break;

    if (path.length() > kMaxPathLength)
      continue;

    result.insert(std::move(path));
  }
  return std::move(result).extract();
}
}  // namespace

namespace web_app {

WebAppOriginAssociationManager::Task::Task(
    const GURL& manifest_url,
    apps::UrlHandlers url_handlers,
    WebAppOriginAssociationManager& manager,
    OnDidGetWebAppOriginAssociations callback)
    : manifest_url_(manifest_url),
      owner_(manager),
      callback_(std::move(callback)) {
  for (auto& url_handler : url_handlers)
    pending_url_handlers_.push_back(std::move(url_handler));
  url_handlers.clear();
}

WebAppOriginAssociationManager::Task::~Task() = default;

void WebAppOriginAssociationManager::Task::Start() {
  if (pending_url_handlers_.empty()) {
    Finalize();
    return;
  }

  FetchAssociationFile(GetCurrentUrlHandler());
}

apps::UrlHandlerInfo&
WebAppOriginAssociationManager::Task::GetCurrentUrlHandler() {
  DCHECK(!pending_url_handlers_.empty());
  return pending_url_handlers_.front();
}

void WebAppOriginAssociationManager::Task::FetchAssociationFile(
    apps::UrlHandlerInfo& url_handler) {
  if (url_handler.origin.Serialize().empty()) {
    MaybeStartNextUrlHandler();
    return;
  }

  owner_->GetFetcher().FetchWebAppOriginAssociationFile(
      url_handler, g_browser_process->shared_url_loader_factory(),
      base::BindOnce(
          &WebAppOriginAssociationManager::Task::OnAssociationFileFetched,
          weak_ptr_factory_.GetWeakPtr()));
}

void WebAppOriginAssociationManager::Task::OnAssociationFileFetched(
    std::unique_ptr<std::string> file_content) {
  if (!file_content || file_content->empty()) {
    MaybeStartNextUrlHandler();
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
    MaybeStartNextUrlHandler();
    return;
  }

  auto& url_handler = GetCurrentUrlHandler();
  for (auto& app : association->apps) {
    if (app->manifest_url == manifest_url_) {
      if (app->paths.has_value())
        url_handler.paths = GetValidPaths(std::move(app->paths.value()));

      if (app->exclude_paths.has_value()) {
        url_handler.exclude_paths =
            GetValidPaths(std::move(app->exclude_paths.value()));
      }

      result_.push_back(std::move(url_handler));
      url_handler.Reset();
      // Only information in the first valid app is saved.
      break;
    }
  }
  association->apps.clear();

  MaybeStartNextUrlHandler();
}

void WebAppOriginAssociationManager::Task::MaybeStartNextUrlHandler() {
  DCHECK(!pending_url_handlers_.empty());
  pending_url_handlers_.pop_front();
  if (!pending_url_handlers_.empty()) {
    // Continue with the next url handler.
    FetchAssociationFile(GetCurrentUrlHandler());
    return;
  }

  Finalize();
}

void WebAppOriginAssociationManager::Task::Finalize() {
  apps::UrlHandlers result = std::move(result_);
  result_.clear();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), std::move(result)));
  owner_->OnTaskCompleted();
}

}  // namespace web_app
