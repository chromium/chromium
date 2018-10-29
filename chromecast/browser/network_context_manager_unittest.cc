// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/network_context_manager.h"

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/network_service.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {

class NetworkContextManagerTest : public testing::Test {
 public:
  NetworkContextManagerTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}
  ~NetworkContextManagerTest() override {}

  void SetUp() override {
    // Create a fake URLRequestContextGetter on the IO thread.
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(
            &NetworkContextManagerTest::CreateURLRequestContextGetterOnIOThread,
            base::Unretained(this)));
    content::RunAllTasksUntilIdle();

    // Create |manager_| on the main thread.
    manager_.reset(NetworkContextManager::CreateForTest(
        url_request_context_getter_,
        network::NetworkService::CreateForTesting()));
  }

  void TearDown() override {
    content::BrowserThread::DeleteSoon(content::BrowserThread::IO, FROM_HERE,
                                       manager_.release());
  }

 protected:
  // This contains a base::test::ScopedTaskEnvironment and allows access to mock
  // the IO thread.
  content::TestBrowserThreadBundle thread_bundle_;

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
  std::unique_ptr<NetworkContextManager> manager_;

 private:
  void CreateURLRequestContextGetterOnIOThread() {
    url_request_context_getter_ = new net::TestURLRequestContextGetter(
        base::ThreadTaskRunnerHandle::Get());
  }

  DISALLOW_COPY_AND_ASSIGN(NetworkContextManagerTest);
};

TEST_F(NetworkContextManagerTest, TestGetURLLoaderFactory) {
  // Create a URLLoaderFactory that's bound to the main thread.
  network::mojom::URLLoaderFactoryPtr factory = manager_->GetURLLoaderFactory();

  network::ResourceRequest request;
  request.url = GURL("https://www.google.com");
  network::mojom::URLLoaderPtr loader;
  network::TestURLLoaderClient loader_client;

  // Make an API call on |factory|.
  factory->CreateLoaderAndStart(
      mojo::MakeRequest(&loader), 0, 0, network::mojom::kURLLoadOptionNone,
      request, loader_client.CreateInterfacePtr(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  // Wait for the request to finish.
  content::RunAllTasksUntilIdle();
}

}  // namespace chromecast
