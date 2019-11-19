// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/test_http_bridge_factory.h"

namespace browser_sync {

bool TestHttpBridge::MakeSynchronousPost(int* net_error_code,
                                         int* http_status_code) {
  return false;
}

int TestHttpBridge::GetResponseContentLength() const {
  return 0;
}

const char* TestHttpBridge::GetResponseContent() const {
  return nullptr;
}

const std::string TestHttpBridge::GetResponseHeaderValue(
    const std::string&) const {
  return std::string();
}

void TestHttpBridge::Abort() {}

TestHttpBridgeFactory::TestHttpBridgeFactory() {}

TestHttpBridgeFactory::~TestHttpBridgeFactory() {}

syncer::HttpPostProviderInterface* TestHttpBridgeFactory::Create() {
  return new TestHttpBridge();
}

void TestHttpBridgeFactory::Destroy(syncer::HttpPostProviderInterface* http) {
  delete static_cast<TestHttpBridge*>(http);
}

}  // namespace browser_sync
