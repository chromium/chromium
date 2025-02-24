// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_URL_LOADER_FACTORY_GETTER_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_URL_LOADER_FACTORY_GETTER_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace download {

using URLLoaderFactoryGetterCallback =
    base::OnceCallback<void(scoped_refptr<network::SharedURLLoaderFactory>)>;

class COMPONENT_EXPORT(COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE)
    URLLoaderFactoryGetter {
 public:
  virtual void RetrieveURLLoaderFactory(
      URLLoaderFactoryGetterCallback callback) = 0;

  URLLoaderFactoryGetter(const URLLoaderFactoryGetter&) = delete;
  URLLoaderFactoryGetter& operator=(const URLLoaderFactoryGetter&) = delete;

  virtual ~URLLoaderFactoryGetter() = default;

 protected:
  URLLoaderFactoryGetter() = default;
};

using URLLoaderFactoryGetterPtr = std::unique_ptr<URLLoaderFactoryGetter>;

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_URL_LOADER_FACTORY_GETTER_H_
