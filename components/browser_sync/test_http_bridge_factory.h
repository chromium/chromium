// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_TEST_HTTP_BRIDGE_FACTORY_H_
#define COMPONENTS_BROWSER_SYNC_TEST_HTTP_BRIDGE_FACTORY_H_

#include <string>

#include "base/compiler_specific.h"
#include "components/sync/engine/net/http_post_provider_factory.h"
#include "components/sync/engine/net/http_post_provider_interface.h"

namespace browser_sync {

class TestHttpBridge : public syncer::HttpPostProviderInterface {
 public:
  // Begin syncer::HttpPostProviderInterface implementation:
  void SetExtraRequestHeaders(const char* headers) override {}

  void SetURL(const char* url, int port) override {}

  void SetPostPayload(const char* content_type,
                      int content_length,
                      const char* content) override {}

  bool MakeSynchronousPost(int* net_error_code, int* http_status_code) override;

  int GetResponseContentLength() const override;

  const char* GetResponseContent() const override;

  const std::string GetResponseHeaderValue(const std::string&) const override;

  void Abort() override;
  // End syncer::HttpPostProviderInterface implementation.
};

class TestHttpBridgeFactory : public syncer::HttpPostProviderFactory {
 public:
  TestHttpBridgeFactory();
  ~TestHttpBridgeFactory() override;

  // syncer::HttpPostProviderFactory:
  syncer::HttpPostProviderInterface* Create() override;
  void Destroy(syncer::HttpPostProviderInterface* http) override;
};

}  // namespace browser_sync

#endif  // COMPONENTS_BROWSER_SYNC_TEST_HTTP_BRIDGE_FACTORY_H_
