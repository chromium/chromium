// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_NACL_PNACL_HEADER_TEST_H_
#define CHROME_TEST_NACL_PNACL_HEADER_TEST_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/common/resource_type.h"

namespace net {
namespace test_server {
struct HttpRequest;
class HttpResponse;
}
}

/*
TODO(https://crbug.com/711289): port to work with network service if this
check matters.

using content::ResourceDispatcherHostDelegate;

class TestDispatcherHostDelegate : public ResourceDispatcherHostDelegate {
 public:
  TestDispatcherHostDelegate()
      : ResourceDispatcherHostDelegate(), found_pnacl_header_(false) {}

  ~TestDispatcherHostDelegate() override {}

  void RequestBeginning(net::URLRequest* request,
                        content::ResourceContext* resource_context,
                        content::AppCacheService* appcache_service,
                        content::ResourceType resource_type,
                        std::vector<std::unique_ptr<content::ResourceThrottle>>*
                            throttles) override;

  bool found_pnacl_header() const { return found_pnacl_header_; }

 private:
  bool found_pnacl_header_;

  DISALLOW_COPY_AND_ASSIGN(TestDispatcherHostDelegate);
};
*/

class PnaclHeaderTest : public InProcessBrowserTest {
 public:
  PnaclHeaderTest();
  ~PnaclHeaderTest() override;

  // Run a simple test that checks that the NaCl plugin sends the right
  // headers when doing |expected_noncors| same origin pexe load requests
  // and |expected_cors| cross origin pexe load requests.
  void RunLoadTest(const std::string& url,
                   int expected_noncors,
                   int expected_cors);

 private:
  void StartServer();

  std::unique_ptr<net::test_server::HttpResponse> WatchForPexeFetch(
      const net::test_server::HttpRequest& request);

  int noncors_loads_;
  int cors_loads_;
  // TestDispatcherHostDelegate test_delegate_;
  DISALLOW_COPY_AND_ASSIGN(PnaclHeaderTest);
};

#endif  // CHROME_TEST_NACL_PNACL_HEADER_TEST_H_
