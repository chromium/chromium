// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_URL_DOWNLOAD_HANDLER_FACTORY_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_URL_DOWNLOAD_HANDLER_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "components/download/public/common/url_download_handler.h"

namespace net {
class URLRequestContextGetter;
}

namespace download {
class DownloadURLLoaderFactoryGetter;
class DownloadUrlParameters;

// Class for handling the creation of a URLDownloadHandler. This is used to
// allow injection of different URLDownloadHandler implementations.
// TODO(qinmin): remove this factory once network service is fully enabled.
class COMPONENTS_DOWNLOAD_EXPORT UrlDownloadHandlerFactory {
 public:
  // Installs a new factory for creating the URLDownloadHandler. Can be called
  // on any thread.
  static void Install(UrlDownloadHandlerFactory* factory);

  // Creates a URLDownloadHandler. By default the handler is used for network
  // service. Must be called on the IO thread.
  static UrlDownloadHandler::UniqueUrlDownloadHandlerPtr Create(
      std::unique_ptr<download::DownloadUrlParameters> params,
      base::WeakPtr<download::UrlDownloadHandler::Delegate> delegate,
      scoped_refptr<download::DownloadURLLoaderFactoryGetter>
          url_loader_factory_getter,
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  virtual ~UrlDownloadHandlerFactory();

 protected:
  UrlDownloadHandlerFactory();

  // To be implemented by child classes to provide their own URLDownloadHandler
  // instances.
  virtual UrlDownloadHandler::UniqueUrlDownloadHandlerPtr
  CreateUrlDownloadHandler(
      std::unique_ptr<download::DownloadUrlParameters> params,
      base::WeakPtr<download::UrlDownloadHandler::Delegate> delegate,
      scoped_refptr<download::DownloadURLLoaderFactoryGetter>
          url_loader_factory_getter,
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) = 0;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_URL_DOWNLOAD_HANDLER_FACTORY_H_
