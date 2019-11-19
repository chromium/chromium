// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/url_loader_factory_provider.h"

#include "components/download/public/common/download_task_runner.h"

namespace download {

URLLoaderFactoryProvider::URLLoaderFactoryProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)) {}

URLLoaderFactoryProvider::~URLLoaderFactoryProvider() = default;

scoped_refptr<network::SharedURLLoaderFactory>
URLLoaderFactoryProvider::GetURLLoaderFactory() {
  return url_loader_factory_;
}

// static
URLLoaderFactoryProvider::URLLoaderFactoryProviderPtr
URLLoaderFactoryProvider::GetNullPtr() {
  return URLLoaderFactoryProvider::URLLoaderFactoryProviderPtr(
      nullptr, base::OnTaskRunnerDeleter(nullptr));
}

}  // namespace download
