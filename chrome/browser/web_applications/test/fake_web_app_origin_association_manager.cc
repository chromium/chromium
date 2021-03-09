// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/fake_web_app_origin_association_manager.h"

#include <utility>

#include "base/threading/sequenced_task_runner_handle.h"
#include "url/gurl.h"

namespace web_app {

FakeWebAppOriginAssociationManager::FakeWebAppOriginAssociationManager() =
    default;

FakeWebAppOriginAssociationManager::~FakeWebAppOriginAssociationManager() =
    default;

void FakeWebAppOriginAssociationManager::GetWebAppOriginAssociations(
    const GURL& manifest_url,
    apps::UrlHandlers url_handlers,
    OnDidGetWebAppOriginAssociations callback) {
  apps::UrlHandlers result;

  if (pass_through_) {
    result = url_handlers;
  } else {
    for (const auto& url_handler : url_handlers) {
      auto it = data_.find(url_handler);
      if (it != data_.end())
        result.push_back(it->second);
    }
  }
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void FakeWebAppOriginAssociationManager::SetData(
    std::map<apps::UrlHandlerInfo, apps::UrlHandlerInfo> data) {
  data_ = std::move(data);
}

}  // namespace web_app
