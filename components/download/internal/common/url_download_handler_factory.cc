// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/url_download_handler_factory.h"

#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "components/download/internal/common/resource_downloader.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/service_manager/public/cpp/connector.h"

namespace download {

// static
UrlDownloadHandler::UniqueUrlDownloadHandlerPtr
UrlDownloadHandlerFactory::Create(
    std::unique_ptr<download::DownloadUrlParameters> params,
    base::WeakPtr<download::UrlDownloadHandler::Delegate> delegate,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const URLSecurityPolicy& url_security_policy,
    std::unique_ptr<service_manager::Connector> connector,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  std::unique_ptr<network::ResourceRequest> request =
      CreateResourceRequest(params.get());
  return UrlDownloadHandler::UniqueUrlDownloadHandlerPtr(
      download::ResourceDownloader::BeginDownload(
          delegate, std::move(params), std::move(request),
          std::move(url_loader_factory), url_security_policy, GURL(), GURL(),
          GURL(), true, true, std::move(connector),
          false /* is_background_mode */, task_runner)
          .release(),
      base::OnTaskRunnerDeleter(base::ThreadTaskRunnerHandle::Get()));
}

}  // namespace download
