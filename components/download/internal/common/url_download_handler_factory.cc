// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/url_download_handler_factory.h"

#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "components/download/internal/common/resource_downloader.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace download {

// static
UrlDownloadHandler::UniqueUrlDownloadHandlerPtr
UrlDownloadHandlerFactory::Create(
    std::unique_ptr<download::DownloadUrlParameters> params,
    base::WeakPtr<download::UrlDownloadHandler::Delegate> delegate,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const URLSecurityPolicy& url_security_policy,
    mojo::PendingRemote<device::mojom::WakeLockProvider> wake_lock_provider,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  std::unique_ptr<network::ResourceRequest> request =
      CreateResourceRequest(params.get());
  return UrlDownloadHandler::UniqueUrlDownloadHandlerPtr(
      download::ResourceDownloader::BeginDownload(
          delegate, std::move(params), std::move(request),
          std::move(url_loader_factory), url_security_policy, std::string(),
          GURL(), GURL(), true, true, std::move(wake_lock_provider),
          false /* is_background_mode */, task_runner)
          .release(),
      base::OnTaskRunnerDeleter(
          base::SingleThreadTaskRunner::GetCurrentDefault()));
}

}  // namespace download
