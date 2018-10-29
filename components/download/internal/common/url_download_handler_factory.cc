// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/url_download_handler_factory.h"

#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "components/download/internal/common/resource_downloader.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_url_loader_factory_getter.h"
#include "components/download/public/common/download_utils.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace download {

namespace {

// Factory for creating URLDownloadHandler used by network service.
class DefaultUrlDownloadHandlerFactory : public UrlDownloadHandlerFactory {
 public:
  DefaultUrlDownloadHandlerFactory() = default;
  ~DefaultUrlDownloadHandlerFactory() override = default;

 protected:
  UrlDownloadHandler::UniqueUrlDownloadHandlerPtr CreateUrlDownloadHandler(
      std::unique_ptr<download::DownloadUrlParameters> params,
      base::WeakPtr<download::UrlDownloadHandler::Delegate> delegate,
      scoped_refptr<download::DownloadURLLoaderFactoryGetter>
          url_loader_factory_getter,
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) override {
    std::unique_ptr<network::ResourceRequest> request =
        CreateResourceRequest(params.get());
    return UrlDownloadHandler::UniqueUrlDownloadHandlerPtr(
        download::ResourceDownloader::BeginDownload(
            delegate, std::move(params), std::move(request),
            std::move(url_loader_factory_getter), GURL(), GURL(), GURL(), true,
            true, task_runner)
            .release(),
        base::OnTaskRunnerDeleter(base::ThreadTaskRunnerHandle::Get()));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultUrlDownloadHandlerFactory);
};

UrlDownloadHandlerFactory* g_url_download_handler_factory;

// Lock to protect |g_url_download_handler_factory|
base::Lock& GetURLDownloadHandlerFactoryLock() {
  static base::NoDestructor<base::Lock> instance;
  return *instance;
}

}  // namespace

// static
UrlDownloadHandler::UniqueUrlDownloadHandlerPtr
UrlDownloadHandlerFactory::Create(
    std::unique_ptr<download::DownloadUrlParameters> params,
    base::WeakPtr<download::UrlDownloadHandler::Delegate> delegate,
    scoped_refptr<download::DownloadURLLoaderFactoryGetter>
        url_loader_factory_getter,
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  base::AutoLock auto_lock(GetURLDownloadHandlerFactoryLock());
  if (!g_url_download_handler_factory)
    g_url_download_handler_factory = new DefaultUrlDownloadHandlerFactory();
  return g_url_download_handler_factory->CreateUrlDownloadHandler(
      std::move(params), delegate, std::move(url_loader_factory_getter),
      std::move(url_request_context_getter), task_runner);
}

// static
void UrlDownloadHandlerFactory::Install(UrlDownloadHandlerFactory* factory) {
  base::AutoLock auto_lock(GetURLDownloadHandlerFactoryLock());
  if (factory == g_url_download_handler_factory)
    return;
  delete g_url_download_handler_factory;
  g_url_download_handler_factory = factory;
}

UrlDownloadHandlerFactory::UrlDownloadHandlerFactory() = default;

UrlDownloadHandlerFactory::~UrlDownloadHandlerFactory() = default;

}  // namespace download
