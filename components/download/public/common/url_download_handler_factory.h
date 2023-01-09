// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_URL_DOWNLOAD_HANDLER_FACTORY_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_URL_DOWNLOAD_HANDLER_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "components/download/public/common/download_utils.h"
#include "components/download/public/common/url_download_handler.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"

namespace download {
class DownloadUrlParameters;

// Class for handling the creation of a URLDownloadHandler. This is used to
// allow injection of different URLDownloadHandler implementations.
// TODO(qinmin): remove this factory once network service is fully enabled.
class COMPONENTS_DOWNLOAD_EXPORT UrlDownloadHandlerFactory {
 public:
  // Creates a URLDownloadHandler. By default the handler is used for network
  // service. Must be called on the IO thread.
  static UrlDownloadHandler::UniqueUrlDownloadHandlerPtr Create(
      std::unique_ptr<download::DownloadUrlParameters> params,
      base::WeakPtr<download::UrlDownloadHandler::Delegate> delegate,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const URLSecurityPolicy& url_security_policy,
      mojo::PendingRemote<device::mojom::WakeLockProvider> wake_lock_provider,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_URL_DOWNLOAD_HANDLER_FACTORY_H_
