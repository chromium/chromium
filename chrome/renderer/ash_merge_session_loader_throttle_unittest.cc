// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/ash_merge_session_loader_throttle.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "chrome/renderer/chrome_render_thread_observer.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "url/gurl.h"

using ::testing::NiceMock;
using ::testing::Return;

class MockChromeOSListener
    : public ChromeRenderThreadObserver::ChromeOSListener {
 public:
  MockChromeOSListener(const MockChromeOSListener&) = delete;
  MockChromeOSListener& operator=(const MockChromeOSListener&) = delete;

  MOCK_METHOD(bool, IsMergeSessionRunning, (), (const, override));
  MOCK_METHOD(void,
              RunWhenMergeSessionFinished,
              (DelayedCallbackGroup::Callback callback),
              (override));

 protected:
  MockChromeOSListener() = default;
  ~MockChromeOSListener() override = default;
};

class AshMergeSessionLoaderThrottleTest : public ::testing::Test {
 public:
  AshMergeSessionLoaderThrottleTest() = default;
  AshMergeSessionLoaderThrottleTest(const AshMergeSessionLoaderThrottleTest&) =
      delete;
  AshMergeSessionLoaderThrottleTest& operator=(
      const AshMergeSessionLoaderThrottleTest&) = delete;
  ~AshMergeSessionLoaderThrottleTest() override = default;

 protected:
  void SetUp() override {
    listener_ = base::MakeRefCounted<NiceMock<MockChromeOSListener>>();
    throttler_ = std::make_unique<AshMergeSessionLoaderThrottle>(listener_);
  }

  void SimulateCookieMintingInProgress() {
    ON_CALL(*listener_.get(), IsMergeSessionRunning())
        .WillByDefault(Return(true));
  }

  network::ResourceRequest CreateGoogleXHRRequest() {
    network::ResourceRequest url_request;
    url_request.resource_type =
        static_cast<int>(blink::mojom::ResourceType::kXhr);
    url_request.url = GURL("https://www.google.com");
    return url_request;
  }

  network::ResourceRequest CreateNonGoogleXHRRequest() {
    network::ResourceRequest url_request;
    url_request.resource_type =
        static_cast<int>(blink::mojom::ResourceType::kXhr);
    url_request.url = GURL("https://www.example.com");
    return url_request;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<MockChromeOSListener> listener_;
  std::unique_ptr<AshMergeSessionLoaderThrottle> throttler_;
};

// Requests to a Google subdomain should be throttled until cookie minting is
// complete.
TEST_F(AshMergeSessionLoaderThrottleTest, RequestsToGoogleShouldBeThrottled) {
  SimulateCookieMintingInProgress();
  network::ResourceRequest url_request = CreateGoogleXHRRequest();
  bool will_be_throttled = false;
  throttler_->WillStartRequest(&url_request, &will_be_throttled);
  EXPECT_TRUE(will_be_throttled);
}

// Requests to a non-Google resource should not be throttled even if cookie
// minting is in progress.
TEST_F(AshMergeSessionLoaderThrottleTest,
       RequestsToNonGoogleResourcesShouldNotBeThrottled) {
  SimulateCookieMintingInProgress();
  network::ResourceRequest url_request = CreateNonGoogleXHRRequest();

  // `will_be_throttled` is `false` here and should remain `false` after
  // `AshMergeSessionLoaderThrottle::WillStartRequest()` is called.
  // `AshMergeSessionLoaderThrottle::WillStartRequest()` doesn't change the
  // value of `will_be_throttled` if the request will not be throttled.
  bool will_be_throttled = false;
  throttler_->WillStartRequest(&url_request, &will_be_throttled);
  EXPECT_FALSE(will_be_throttled);
}
