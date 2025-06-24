// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/composebox/composebox_handler.h"

#include "base/version_info/channel.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/composebox.mojom.h"
#include "components/omnibox/composebox/composebox_query_controller.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockQueryController : public ComposeboxQueryController {
 public:
  explicit MockQueryController(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      version_info::Channel channel)
      : ComposeboxQueryController(identity_manager,
                                  url_loader_factory,
                                  channel) {}
  ~MockQueryController() override = default;

  MOCK_METHOD(void, NotifySessionStarted, ());
  MOCK_METHOD(void, NotifySessionAbandoned, ());
};

class ComposeboxHandlerTest : public testing::Test {
 public:
  ComposeboxHandlerTest() = default;
  ~ComposeboxHandlerTest() override = default;

  void SetUp() override {
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_factory_);
    auto query_controller_ptr = std::make_unique<MockQueryController>(
        /*identity_manager=*/nullptr, shared_url_loader_factory_,
        version_info::Channel::UNKNOWN);
    query_controller_ = query_controller_ptr.get();
    handler_ = std::make_unique<ComposeboxHandler>(
        mojo::PendingReceiver<composebox::mojom::ComposeboxPageHandler>(),
        std::move(query_controller_ptr));
  }

  ComposeboxHandler& handler() { return *handler_; }
  MockQueryController& query_controller() { return *query_controller_; }

 private:
  std::unique_ptr<ComposeboxHandler> handler_;
  network::TestURLLoaderFactory test_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  raw_ptr<MockQueryController> query_controller_;
};

TEST_F(ComposeboxHandlerTest, NotifySessionStarted) {
  EXPECT_CALL(query_controller(), NotifySessionStarted).Times(1);
  handler().NotifySessionStarted();
}

TEST_F(ComposeboxHandlerTest, NotifySessionAbandoned) {
  EXPECT_CALL(query_controller(), NotifySessionAbandoned).Times(1);
  handler().NotifySessionAbandoned();
}
