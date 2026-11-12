// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/fcm/engine/chrome_build_info.h"
#include "components/gcm_driver/fake_gcm_app_handler.h"
#include "components/gcm_driver/fake_gcm_server.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "components/gcm_driver/gcm_driver_desktop.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/base/network_change_notifier.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/proxy_resolving_socket.mojom.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

constexpr char kTestAppId1[] = "TestApp1";
constexpr char kTestAppId2[] = "TestApp2";
constexpr char kAuthorizedEntity[] = "987654321";
constexpr char kScope[] = "GCM";
constexpr char kProductCategoryForSubtypes[] = "com.chrome.linux";

// End-to-end integration tests exercising `GCMDriverDesktop`,
// `GCMClientFactory` (`GCMClientImpl`), `InstanceIDDriver`, and `GCMStoreImpl`
// over loopback HTTP (`EmbeddedTestServer`) and TCP (`FakeMCSServer`) against
// `FakeGCMServer`.
class GCMDriverDesktopIntegrationTest : public testing::Test {
 public:
  GCMDriverDesktopIntegrationTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        network_change_notifier_(
            net::NetworkChangeNotifier::CreateMockIfNeeded()),
        os_crypt_(os_crypt_async::GetTestOSCryptAsyncForTesting(
            /*is_sync_for_unittests=*/true)),
        shared_url_loader_factory_(
            base::MakeRefCounted<network::TestSharedURLLoaderFactory>()) {
    CHECK(client_store_dir_.CreateUniqueTempDir());
    CHECK(server_store_dir_.CreateUniqueTempDir());
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        net::NetworkChangeNotifier::CONNECTION_WIFI);

    StartNewServer();
    CreateClient();
  }

  ~GCMDriverDesktopIntegrationTest() override { DestroyClient(); }

  void StartNewServer() {
    embedded_test_server_ = std::make_unique<net::EmbeddedTestServer>();
    CHECK(embedded_test_server_->InitializeAndListen());

    fake_gcm_server_ = std::make_unique<FakeGCMServer>();
    fake_gcm_server_->InitializeAndListen();
    fake_gcm_server_->RegisterRequestHandler(*embedded_test_server_);
    fake_gcm_server_->AppendCommandLineSwitches(
        scoped_command_line_.GetProcessCommandLine(), *embedded_test_server_);
    fake_gcm_server_->Start(server_store_dir_.GetPath());

    embedded_test_server_->StartAcceptingConnections();
  }

  void DestroyServer() {
    embedded_test_server_.reset();
    fake_gcm_server_.reset();
  }

  void CreateClient() {
    fcm::ChromeBuildInfo chrome_build_info;
    chrome_build_info.product_category_for_subtypes =
        kProductCategoryForSubtypes;
    chrome_build_info.version = "1.0.0.0";

    driver_ = std::make_unique<GCMDriverDesktop>(
        std::make_unique<GCMClientFactory>(), chrome_build_info,
        client_store_dir_.GetPath(),
        base::BindRepeating(
            &GCMDriverDesktopIntegrationTest::CreateProxyResolvingSocketFactory,
            base::Unretained(this)),
        shared_url_loader_factory_,
        network::TestNetworkConnectionTracker::GetInstance(),
        task_environment_.GetMainThreadTaskRunner(),
        task_environment_.GetMainThreadTaskRunner(),
        task_environment_.GetMainThreadTaskRunner(), os_crypt_.get());
    instance_id_driver_ =
        std::make_unique<instance_id::InstanceIDDriver>(driver_.get());
    app_handler1_ = std::make_unique<FakeGCMAppHandler>();
    app_handler2_ = std::make_unique<FakeGCMAppHandler>();
    driver_->AddAppHandler(kTestAppId1, app_handler1_.get());
    driver_->AddAppHandler(kTestAppId2, app_handler2_.get());
  }

  void DestroyClient() {
    if (!driver_) {
      return;
    }
    driver_->RemoveAppHandler(kTestAppId1);
    driver_->RemoveAppHandler(kTestAppId2);
    instance_id_driver_.reset();
    driver_->Shutdown();
    driver_.reset();
    app_handler1_.reset();
    app_handler2_.reset();

    // Flush the two hops on `GetMainThreadTaskRunner()` (`IOWorker` teardown
    // posting `GCMStoreImpl::Backend` close/release) so the LevelDB lock is
    // released before reopening or deleting `client_store_dir_`.
    for (int i = 0; i < 2; ++i) {
      base::RunLoop run_loop;
      task_environment_.GetMainThreadTaskRunner()->PostTask(
          FROM_HERE, run_loop.QuitClosure());
      run_loop.Run();
    }
  }

  std::pair<std::string, instance_id::InstanceID::Result> GetToken(
      const std::string& app_id,
      const std::string& authorized_entity,
      const std::string& scope) {
    base::test::TestFuture<const std::string&, instance_id::InstanceID::Result>
        future;
    instance_id_driver_->GetInstanceID(app_id)->GetToken(
        authorized_entity, scope, /*time_to_live=*/base::TimeDelta(),
        /*flags=*/{}, future.GetCallback());
    return future.Take();
  }

  instance_id::InstanceID::Result DeleteToken(
      const std::string& app_id,
      const std::string& authorized_entity,
      const std::string& scope) {
    base::test::TestFuture<instance_id::InstanceID::Result> future;
    instance_id_driver_->GetInstanceID(app_id)->DeleteToken(
        authorized_entity, scope, future.GetCallback());
    return future.Get();
  }

  instance_id::InstanceID::Result DeleteID(const std::string& app_id) {
    base::test::TestFuture<instance_id::InstanceID::Result> future;
    instance_id_driver_->GetInstanceID(app_id)->DeleteID(future.GetCallback());
    return future.Get();
  }

  FakeGCMServer* fake_gcm_server() { return fake_gcm_server_.get(); }
  FakeGCMAppHandler* app_handler1() { return app_handler1_.get(); }
  FakeGCMAppHandler* app_handler2() { return app_handler2_.get(); }

 private:
  void CreateProxyResolvingSocketFactory(
      mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
          receiver) {
    shared_url_loader_factory_->network_context()
        ->CreateProxyResolvingSocketFactory(std::move(receiver));
  }

  base::ScopedTempDir client_store_dir_;
  base::ScopedTempDir server_store_dir_;
  base::test::ScopedCommandLine scoped_command_line_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_;
  scoped_refptr<network::TestSharedURLLoaderFactory> shared_url_loader_factory_;

  std::unique_ptr<FakeGCMServer> fake_gcm_server_;
  std::unique_ptr<net::EmbeddedTestServer> embedded_test_server_;

  std::unique_ptr<GCMDriverDesktop> driver_;
  std::unique_ptr<instance_id::InstanceIDDriver> instance_id_driver_;
  std::unique_ptr<FakeGCMAppHandler> app_handler1_;
  std::unique_ptr<FakeGCMAppHandler> app_handler2_;
};

TEST_F(GCMDriverDesktopIntegrationTest, InstanceIDGetTokenAndReceiveMessage) {
  auto [token, result] = GetToken(kTestAppId1, kAuthorizedEntity, kScope);
  ASSERT_EQ(result, instance_id::InstanceID::SUCCESS);
  ASSERT_FALSE(token.empty());

  // Requesting the token again with the same parameters returns the same token.
  auto [second_token, second_result] =
      GetToken(kTestAppId1, kAuthorizedEntity, kScope);
  EXPECT_EQ(second_result, instance_id::InstanceID::SUCCESS);
  EXPECT_EQ(second_token, token);

  ASSERT_TRUE(fake_gcm_server()->SendMessage(token, "iid_payload"));
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return app_handler1()->received_messages().size() == 1u; }));

  EXPECT_EQ(app_handler1()->received_event(), FakeGCMAppHandler::MESSAGE_EVENT);
  EXPECT_EQ(app_handler1()->app_id(), kTestAppId1);
  EXPECT_EQ(app_handler1()->message().sender_id, kAuthorizedEntity);
  EXPECT_EQ(app_handler1()->message().raw_data, "iid_payload");
}

TEST_F(GCMDriverDesktopIntegrationTest,
       InstanceIDDeleteTokenStopsMessageDelivery) {
  auto [token, result] = GetToken(kTestAppId1, kAuthorizedEntity, kScope);
  ASSERT_EQ(result, instance_id::InstanceID::SUCCESS);
  ASSERT_FALSE(token.empty());

  EXPECT_EQ(DeleteToken(kTestAppId1, kAuthorizedEntity, kScope),
            instance_id::InstanceID::SUCCESS);
  EXPECT_FALSE(fake_gcm_server()->SendMessage(token, "after_delete_token"));
}

TEST_F(GCMDriverDesktopIntegrationTest,
       InstanceIDDeleteIDStopsMessageDelivery) {
  auto [token, result] = GetToken(kTestAppId1, kAuthorizedEntity, kScope);
  ASSERT_EQ(result, instance_id::InstanceID::SUCCESS);
  ASSERT_FALSE(token.empty());

  EXPECT_EQ(DeleteID(kTestAppId1), instance_id::InstanceID::SUCCESS);
  EXPECT_FALSE(fake_gcm_server()->SendMessage(token, "after_delete_id"));
}

TEST_F(GCMDriverDesktopIntegrationTest, MultipleAppsAndTokensIsolatedDelivery) {
  auto [token1, result1] = GetToken(kTestAppId1, kAuthorizedEntity, kScope);
  ASSERT_EQ(result1, instance_id::InstanceID::SUCCESS);
  auto [token2, result2] = GetToken(kTestAppId2, kAuthorizedEntity, kScope);
  ASSERT_EQ(result2, instance_id::InstanceID::SUCCESS);
  EXPECT_NE(token1, token2);

  ASSERT_TRUE(fake_gcm_server()->SendMessage(token2, "for_app_2"));
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return app_handler2()->received_messages().size() == 1u; }));
  EXPECT_EQ(app_handler2()->received_event(), FakeGCMAppHandler::MESSAGE_EVENT);
  EXPECT_EQ(app_handler2()->app_id(), kTestAppId2);
  EXPECT_EQ(app_handler2()->message().raw_data, "for_app_2");
  EXPECT_EQ(app_handler1()->received_event(), FakeGCMAppHandler::NO_EVENT);

  ASSERT_TRUE(fake_gcm_server()->SendMessage(token1, "for_app_1"));
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return app_handler1()->received_messages().size() == 1u; }));
  EXPECT_EQ(app_handler1()->received_event(), FakeGCMAppHandler::MESSAGE_EVENT);
  EXPECT_EQ(app_handler1()->app_id(), kTestAppId1);
  EXPECT_EQ(app_handler1()->message().raw_data, "for_app_1");
}

TEST_F(GCMDriverDesktopIntegrationTest,
       DisconnectAndReconnectDeliversQueuedMessage) {
  auto [token, result] = GetToken(kTestAppId1, kAuthorizedEntity, kScope);
  ASSERT_EQ(result, instance_id::InstanceID::SUCCESS);

  // Deliver an initial message to ensure the MCS TCP stream is established.
  ASSERT_TRUE(fake_gcm_server()->SendMessage(token, "initial_message"));
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return app_handler1()->received_messages().size() == 1u; }));
  ASSERT_EQ(app_handler1()->message().raw_data, "initial_message");

  // Simulate going offline and severing the MCS connection.
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  fake_gcm_server()->DisconnectAllClients();

  // Queue a message while the client is disconnected.
  ASSERT_TRUE(fake_gcm_server()->SendMessage(token, "queued_while_offline"));

  // Restore connectivity; the client should reconnect and receive the message.
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return app_handler1()->received_messages().size() == 2u; }));
  EXPECT_EQ(app_handler1()->received_event(), FakeGCMAppHandler::MESSAGE_EVENT);
  EXPECT_EQ(app_handler1()->message().raw_data, "queued_while_offline");
}

TEST_F(GCMDriverDesktopIntegrationTest,
       PersistenceAcrossClientAndServerRestart) {
  auto [token, result] = GetToken(kTestAppId1, kAuthorizedEntity, kScope);
  ASSERT_EQ(result, instance_id::InstanceID::SUCCESS);

  // Deliver an initial message so the client and server exchange acks.
  ASSERT_TRUE(fake_gcm_server()->SendMessage(token, "before_restart"));
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return app_handler1()->received_messages().size() == 1u; }));
  ASSERT_EQ(app_handler1()->message().raw_data, "before_restart");

  // Tear down both client and server, then restart the server from disk and
  // queue a message for the persisted registration token before the client
  // reconnects.
  DestroyClient();
  DestroyServer();

  StartNewServer();
  ASSERT_TRUE(fake_gcm_server()->SendMessage(token, "after_restart"));

  // Recreate the client from its persisted store; calling `GetToken` will
  // start `GCMClientImpl` using the persisted checkin credentials and token.
  CreateClient();
  auto [restored_token, restored_result] =
      GetToken(kTestAppId1, kAuthorizedEntity, kScope);
  EXPECT_EQ(restored_result, instance_id::InstanceID::SUCCESS);
  EXPECT_EQ(restored_token, token);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return app_handler1()->received_messages().size() == 1u; }));
  EXPECT_EQ(app_handler1()->received_event(), FakeGCMAppHandler::MESSAGE_EVENT);
  EXPECT_EQ(app_handler1()->message().raw_data, "after_restart");
}

}  // namespace

}  // namespace gcm
