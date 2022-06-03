// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_SIMPLE_URL_LOADER_FACTORY_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_SIMPLE_URL_LOADER_FACTORY_H_

#include <memory>

#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace autofill_assistant {

// Base interface for creators of URL loaders.
class SimpleURLLoaderFactory {
 public:
  virtual ~SimpleURLLoaderFactory() = default;

  virtual std::unique_ptr<::network::SimpleURLLoader> CreateLoader(
      std::unique_ptr<::network::ResourceRequest> resource_request,
      const ::net::NetworkTrafficAnnotationTag& annotation_tag) const = 0;
};

// The native implementation of |SimpleURLLoaderFactory|.
class NativeURLLoaderFactory : public SimpleURLLoaderFactory {
 public:
  NativeURLLoaderFactory() = default;
  ~NativeURLLoaderFactory() override = default;
  NativeURLLoaderFactory(const NativeURLLoaderFactory&) = delete;
  NativeURLLoaderFactory& operator=(const NativeURLLoaderFactory&) = delete;

  std::unique_ptr<::network::SimpleURLLoader> CreateLoader(
      std::unique_ptr<::network::ResourceRequest> resource_request,
      const ::net::NetworkTrafficAnnotationTag& annotation_tag) const override;
};

}  //  namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_SIMPLE_URL_LOADER_FACTORY_H_
