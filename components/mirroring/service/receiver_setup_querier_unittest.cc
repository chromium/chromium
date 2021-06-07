// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/receiver_setup_querier.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/default_tick_clock.h"
#include "components/mirroring/service/value_util.h"
#include "media/cast/cast_environment.h"
#include "media/cast/test/utility/net_utility.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/ip_endpoint.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mirroring {

class ReceiverSetupQuerierTest : public ::testing::Test {
 public:
  ReceiverSetupQuerierTest()
      : receiver_address_(media::cast::test::GetFreeLocalPort().address()) {}
  ~ReceiverSetupQuerierTest() override = default;

 protected:
  void CreateReceiverSetupQuerier() {
    mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory;
    auto test_url_loader_factory =
        std::make_unique<network::TestURLLoaderFactory>();
    url_loader_factory_ = test_url_loader_factory.get();
    mojo::MakeSelfOwnedReceiver(
        std::move(test_url_loader_factory),
        url_loader_factory.InitWithNewPipeAndPassReceiver());

    setup_querier_ = std::make_unique<ReceiverSetupQuerier>(
        receiver_address_, std::move(url_loader_factory));
  }

  void SendReceiverSetupInfo(const std::string& setup_info) {
    url_loader_factory_->AddResponse(
        "http://" + receiver_address_.ToString() + ":8008/setup/eureka_info",
        setup_info);
    task_environment_.RunUntilIdle();
  }

  ReceiverSetupQuerier* setup_querier() { return setup_querier_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  const net::IPAddress receiver_address_;
  network::TestURLLoaderFactory* url_loader_factory_ = nullptr;
  std::unique_ptr<ReceiverSetupQuerier> setup_querier_;

  DISALLOW_COPY_AND_ASSIGN(ReceiverSetupQuerierTest);
};

TEST_F(ReceiverSetupQuerierTest, ValidSetupInfo) {
  const std::string kReceiverSetupInfo =
      R"({ "cast_build_revision": "1.26.0.1",
           "connected": true,
           "ethernet_connected": false,
           "has_update": false,
           "uptime": 13253.6,
           "name": "Eureka"
         })";

  CreateReceiverSetupQuerier();
  SendReceiverSetupInfo(kReceiverSetupInfo);

  EXPECT_EQ("1.26.0.1", setup_querier()->build_version());
  EXPECT_EQ("Eureka", setup_querier()->friendly_name());
}

TEST_F(ReceiverSetupQuerierTest, SetupInfoMissingName) {
  const std::string kReceiverSetupInfo =
      R"({ "cast_build_revision": "1.26.0.1" })";

  CreateReceiverSetupQuerier();
  SendReceiverSetupInfo(kReceiverSetupInfo);

  EXPECT_EQ("1.26.0.1", setup_querier()->build_version());
  EXPECT_EQ("", setup_querier()->friendly_name());
}

TEST_F(ReceiverSetupQuerierTest, SetupInfoMissingRevision) {
  const std::string kReceiverSetupInfo = R"({ "name": "Eureka" })";

  CreateReceiverSetupQuerier();
  SendReceiverSetupInfo(kReceiverSetupInfo);

  EXPECT_EQ("", setup_querier()->build_version());
  EXPECT_EQ("Eureka", setup_querier()->friendly_name());
}

TEST_F(ReceiverSetupQuerierTest, EmptySetupInfo) {
  const std::string kReceiverSetupInfo = "{}";

  CreateReceiverSetupQuerier();
  SendReceiverSetupInfo(kReceiverSetupInfo);

  EXPECT_EQ("", setup_querier()->build_version());
  EXPECT_EQ("", setup_querier()->friendly_name());
}

TEST_F(ReceiverSetupQuerierTest, InfoNeverSent) {
  CreateReceiverSetupQuerier();
  EXPECT_EQ("", setup_querier()->build_version());
  EXPECT_EQ("", setup_querier()->friendly_name());
}

}  // namespace mirroring
