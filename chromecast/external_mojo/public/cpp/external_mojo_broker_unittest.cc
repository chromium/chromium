// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/external_mojo/public/cpp/external_mojo_broker.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "chromecast/external_mojo/public/mojom/connector.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/service_filter.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace external_mojo {

namespace {

constexpr char kRestrictedService[] = "restricted_service";
constexpr char kOpenService[] = "open_service";
constexpr char kInterfaceName[] = "test.mojom.Foo";

class TestService : public mojom::ExternalService {
 public:
  int bind_count() const { return bind_count_; }

  mojo::PendingRemote<mojom::ExternalService> GetRemote() {
    receiver_.reset();
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  void OnBindInterface(const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    ++bind_count_;
  }

  int bind_count_ = 0;
  mojo::Receiver<mojom::ExternalService> receiver_{this};
};

void RegisterService(mojo::Remote<mojom::ExternalConnector>& connector,
                     const std::string& name,
                     TestService* service) {
  std::vector<mojom::ServiceInstanceInfoPtr> instances;
  instances.push_back(
      mojom::ServiceInstanceInfo::New(name, service->GetRemote()));
  connector->RegisterServiceInstances(std::move(instances));
}

void Bind(mojo::Remote<mojom::ExternalConnector>& connector,
          const std::string& service_name) {
  mojo::MessagePipe pipe;
  connector->BindInterface(service_name, kInterfaceName,
                           std::move(pipe.handle0));
}

class ExternalMojoBrokerTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    broker_ = std::make_unique<ExternalMojoBroker>(
        temp_dir_.GetPath().Append("broker").value(),
        std::vector<std::string>{kRestrictedService});
    in_process_.Bind(broker_->CreateConnector());
    broker_->BindExternalConnector(external_.BindNewPipeAndPassReceiver());
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<ExternalMojoBroker> broker_;
  mojo::Remote<mojom::ExternalConnector> in_process_;
  mojo::Remote<mojom::ExternalConnector> external_;
};

TEST_F(ExternalMojoBrokerTest, ExternalClientCannotBindInProcessOnlyService) {
  TestService restricted;
  TestService open;
  RegisterService(in_process_, kRestrictedService, &restricted);
  RegisterService(in_process_, kOpenService, &open);
  task_environment_.RunUntilIdle();

  Bind(external_, kRestrictedService);
  Bind(external_, kOpenService);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, restricted.bind_count());
  EXPECT_EQ(1, open.bind_count());

  Bind(in_process_, kRestrictedService);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, restricted.bind_count());
}

TEST_F(ExternalMojoBrokerTest,
       ExternalClientCannotBindInProcessOnlyServiceViaClone) {
  TestService restricted;
  RegisterService(in_process_, kRestrictedService, &restricted);
  task_environment_.RunUntilIdle();

  mojo::Remote<mojom::ExternalConnector> external_clone;
  external_->Clone(external_clone.BindNewPipeAndPassReceiver());
  Bind(external_clone, kRestrictedService);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, restricted.bind_count());

  mojo::Remote<mojom::ExternalConnector> in_process_clone;
  in_process_->Clone(in_process_clone.BindNewPipeAndPassReceiver());
  Bind(in_process_clone, kRestrictedService);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, restricted.bind_count());
}

TEST_F(ExternalMojoBrokerTest,
       ExternalClientCannotRegisterInProcessOnlyService) {
  TestService impostor;
  RegisterService(external_, kRestrictedService, &impostor);
  task_environment_.RunUntilIdle();

  Bind(in_process_, kRestrictedService);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, impostor.bind_count());

  TestService real;
  RegisterService(in_process_, kRestrictedService, &real);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, real.bind_count());
}

TEST_F(ExternalMojoBrokerTest, PendingExternalBindRejectedOnServiceRegistered) {
  Bind(external_, kRestrictedService);
  Bind(in_process_, kRestrictedService);
  task_environment_.RunUntilIdle();

  TestService restricted;
  RegisterService(in_process_, kRestrictedService, &restricted);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, restricted.bind_count());
}

TEST_F(ExternalMojoBrokerTest,
       ExternalClientCannotBindInProcessOnlyServiceViaChromiumConnector) {
  TestService restricted;
  RegisterService(in_process_, kRestrictedService, &restricted);
  task_environment_.RunUntilIdle();

  mojo::Remote<service_manager::mojom::Connector> sm_connector;
  external_->BindChromiumConnector(
      sm_connector.BindNewPipeAndPassReceiver().PassPipe());
  mojo::MessagePipe pipe;
  sm_connector->BindInterface(
      service_manager::ServiceFilter::ByName(kRestrictedService),
      kInterfaceName, std::move(pipe.handle0),
      service_manager::mojom::BindInterfacePriority::kBestEffort,
      base::DoNothing());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, restricted.bind_count());
}

}  // namespace

}  // namespace external_mojo
}  // namespace chromecast
