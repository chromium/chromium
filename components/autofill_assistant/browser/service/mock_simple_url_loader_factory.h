// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_MOCK_SIMPLE_URL_LOADER_FACTORY_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_MOCK_SIMPLE_URL_LOADER_FACTORY_H_

#include <memory>

#include "components/autofill_assistant/browser/service/simple_url_loader_factory.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class MockSimpleURLLoaderFactory : public SimpleURLLoaderFactory {
 public:
  MockSimpleURLLoaderFactory();
  ~MockSimpleURLLoaderFactory() override;

  std::unique_ptr<::network::SimpleURLLoader> CreateLoader(
      std::unique_ptr<::network::ResourceRequest> resource_request,
      const ::net::NetworkTrafficAnnotationTag& annotation_tag) const override {
    return OnCreateLoader(resource_request.get(), annotation_tag);
  }

  MOCK_CONST_METHOD2(
      OnCreateLoader,
      std::unique_ptr<::network::SimpleURLLoader>(
          ::network::ResourceRequest* resource_request,
          const ::net::NetworkTrafficAnnotationTag& annotation_tag));
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_MOCK_SIMPLE_URL_LOADER_FACTORY_H_
