// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/hsts_query.h"

#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/network_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace password_manager {
namespace {

// Auxiliary class to automatically set and reset the HSTS state for a given
// host.
class HSTSStateManager {
 public:
  HSTSStateManager(net::TransportSecurityState* state,
                   bool is_hsts,
                   std::string host);
  ~HSTSStateManager();

 private:
  net::TransportSecurityState* state_;
  const bool is_hsts_;
  const std::string host_;

  DISALLOW_COPY_AND_ASSIGN(HSTSStateManager);
};

HSTSStateManager::HSTSStateManager(net::TransportSecurityState* state,
                                   bool is_hsts,
                                   std::string host)
    : state_(state), is_hsts_(is_hsts), host_(std::move(host)) {
  if (is_hsts_) {
    base::Time expiry = base::Time::Max();
    bool include_subdomains = false;
    state_->AddHSTS(host_, expiry, include_subdomains);
  }
}

HSTSStateManager::~HSTSStateManager() {
  if (is_hsts_)
    state_->DeleteDynamicDataForHost(host_);
}

}  // namespace

class HSTSQueryTest : public testing::Test {
 public:
  HSTSQueryTest()
      : request_context_(new net::TestURLRequestContextGetter(
            base::ThreadTaskRunnerHandle::Get())),
        network_context_(std::make_unique<network::NetworkContext>(
            nullptr,
            network_context_remote_.BindNewPipeAndPassReceiver(),
            request_context_->GetURLRequestContext(),
            /*cors_exempt_header_list=*/std::vector<std::string>())) {}

  network::NetworkContext* network_context() { return network_context_.get(); }

 private:
  // Used by request_context_.
  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  mojo::Remote<network::mojom::NetworkContext> network_context_remote_;
  std::unique_ptr<network::NetworkContext> network_context_;

  DISALLOW_COPY_AND_ASSIGN(HSTSQueryTest);
};

TEST_F(HSTSQueryTest, TestPostHSTSQueryForHostAndRequestContext) {
  const GURL origin("https://example.org");
  for (bool is_hsts : {false, true}) {
    SCOPED_TRACE(testing::Message()
                 << std::boolalpha << "is_hsts: " << is_hsts);

    HSTSStateManager manager(
        network_context()->url_request_context()->transport_security_state(),
        is_hsts, origin.host());
    // Post query and ensure callback gets run.
    bool callback_ran = false;
    PostHSTSQueryForHostAndNetworkContext(
        origin, network_context(),
        base::BindOnce(
            [](bool* ran, bool is_hsts, password_manager::HSTSResult result) {
              *ran = true;
              EXPECT_EQ(is_hsts ? password_manager::HSTSResult::kYes
                                : password_manager::HSTSResult::kNo,
                        result);
            },
            &callback_ran, is_hsts));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(callback_ran);
  }
}

TEST_F(HSTSQueryTest, NullNetworkContext) {
  const GURL origin("https://example.org");
  bool callback_ran = false;
  PostHSTSQueryForHostAndNetworkContext(
      origin, nullptr,
      base::BindOnce(
          [](bool* ran, password_manager::HSTSResult result) {
            *ran = true;
            EXPECT_EQ(password_manager::HSTSResult::kError, result);
          },
          &callback_ran));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_ran);
}

}  // namespace password_manager
