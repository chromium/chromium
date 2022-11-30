// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_URL_LOADER_FACTORY_PROVIDER_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_URL_LOADER_FACTORY_PROVIDER_H_

#include "base/task/sequenced_task_runner.h"
#include "components/download/public/common/download_export.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace download {

// Class for wrapping a SharedURLLoaderFactory that can be passed across thread
// so that it can be used later on IO thread to retrieve the factory to create
// parallel download requests.
class COMPONENTS_DOWNLOAD_EXPORT URLLoaderFactoryProvider {
 public:
  using URLLoaderFactoryProviderPtr =
      std::unique_ptr<URLLoaderFactoryProvider, base::OnTaskRunnerDeleter>;

  explicit URLLoaderFactoryProvider(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  URLLoaderFactoryProvider(const URLLoaderFactoryProvider&) = delete;
  URLLoaderFactoryProvider& operator=(const URLLoaderFactoryProvider&) = delete;

  virtual ~URLLoaderFactoryProvider();

  // Called on the io thread to get the URL loader.
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory();

  // Helper method to get an null ptr.
  static URLLoaderFactoryProviderPtr GetNullPtr();

 private:
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_URL_LOADER_FACTORY_PROVIDER_H_
