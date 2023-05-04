// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_origin_association_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/web_applications/web_app_origin_association_task.h"
#include "components/webapps/services/web_app_origin_association/web_app_origin_association_fetcher.h"
#include "components/webapps/services/web_app_origin_association/web_app_origin_association_parser_service.h"

namespace web_app {

WebAppOriginAssociationManager::WebAppOriginAssociationManager()
    : fetcher_(std::make_unique<webapps::WebAppOriginAssociationFetcher>()) {}

WebAppOriginAssociationManager::~WebAppOriginAssociationManager() = default;

void WebAppOriginAssociationManager::GetWebAppOriginAssociations(
    const GURL& web_app_identity,
    ScopeExtensions scope_extensions,
    OnDidGetWebAppOriginAssociations callback) {
  if (scope_extensions.empty()) {
    std::move(callback).Run(ScopeExtensions());
    return;
  }

  auto task =
      std::make_unique<Task>(web_app_identity, std::move(scope_extensions),
                             *this, std::move(callback));
  pending_tasks_.push_back(std::move(task));
  MaybeStartNextTask();
}

void WebAppOriginAssociationManager::MaybeStartNextTask() {
  if (task_in_progress_ || pending_tasks_.empty())
    return;

  task_in_progress_ = true;
  pending_tasks_.front().get()->Start();
}

void WebAppOriginAssociationManager::OnTaskCompleted() {
  DCHECK(!pending_tasks_.empty());
  task_in_progress_ = false;
  pending_tasks_.pop_front();
  MaybeStartNextTask();
}

void WebAppOriginAssociationManager::SetFetcherForTest(
    std::unique_ptr<webapps::WebAppOriginAssociationFetcher> fetcher) {
  fetcher_ = std::move(fetcher);
}

webapps::WebAppOriginAssociationFetcher&
WebAppOriginAssociationManager::GetFetcherForTest() {
  DCHECK(fetcher_);
  return *fetcher_;
}

const mojo::Remote<webapps::mojom::WebAppOriginAssociationParser>&
WebAppOriginAssociationManager::GetParser() {
  if (!parser_ || !parser_.is_bound()) {
    parser_ = webapps::LaunchWebAppOriginAssociationParser();
    parser_.reset_on_disconnect();
  }

  return parser_;
}

webapps::WebAppOriginAssociationFetcher&
WebAppOriginAssociationManager::GetFetcher() {
  return *fetcher_;
}

}  // namespace web_app
