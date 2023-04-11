// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/firewall_hole/firewall_hole.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/permission_broker/fake_permission_broker_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

void CopyFirewallHole(base::RunLoop* run_loop,
                      std::unique_ptr<FirewallHole>* out_hole,
                      std::unique_ptr<FirewallHole> hole) {
  *out_hole = std::move(hole);
  run_loop->Quit();
}

class FirewallHoleTest : public testing::Test {
 public:
  FirewallHoleTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI) {}
  ~FirewallHoleTest() override = default;

  void SetUp() override { chromeos::PermissionBrokerClient::InitializeFake(); }

  void TearDown() override { chromeos::PermissionBrokerClient::Shutdown(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(FirewallHoleTest, GrantTcpPortAccess) {
  base::RunLoop run_loop;
  std::unique_ptr<FirewallHole> hole;
  FirewallHole::Open(FirewallHole::PortType::kTcp, 1234, "foo0",
                     base::BindOnce(&CopyFirewallHole, &run_loop, &hole));
  run_loop.Run();
  EXPECT_TRUE(hole.get());
  EXPECT_TRUE(FakePermissionBrokerClient::Get()->HasTcpHole(1234, "foo0"));
  hole.reset();
  EXPECT_FALSE(FakePermissionBrokerClient::Get()->HasTcpHole(1234, "foo0"));
}

TEST_F(FirewallHoleTest, DenyTcpPortAccess) {
  FakePermissionBrokerClient::Get()->AddTcpDenyRule(1234, "foo0");

  base::RunLoop run_loop;
  std::unique_ptr<FirewallHole> hole;
  FirewallHole::Open(FirewallHole::PortType::kTcp, 1234, "foo0",
                     base::BindOnce(&CopyFirewallHole, &run_loop, &hole));
  run_loop.Run();
  EXPECT_FALSE(hole.get());
}

TEST_F(FirewallHoleTest, GrantUdpPortAccess) {
  base::RunLoop run_loop;
  std::unique_ptr<FirewallHole> hole;
  FirewallHole::Open(FirewallHole::PortType::kUdp, 1234, "foo0",
                     base::BindOnce(&CopyFirewallHole, &run_loop, &hole));
  run_loop.Run();
  EXPECT_TRUE(hole.get());
  EXPECT_TRUE(FakePermissionBrokerClient::Get()->HasUdpHole(1234, "foo0"));
  hole.reset();
  EXPECT_FALSE(FakePermissionBrokerClient::Get()->HasUdpHole(1234, "foo0"));
}

TEST_F(FirewallHoleTest, DenyUdpPortAccess) {
  FakePermissionBrokerClient::Get()->AddUdpDenyRule(1234, "foo0");

  base::RunLoop run_loop;
  std::unique_ptr<FirewallHole> hole;
  FirewallHole::Open(FirewallHole::PortType::kUdp, 1234, "foo0",
                     base::BindOnce(&CopyFirewallHole, &run_loop, &hole));
  run_loop.Run();
  EXPECT_FALSE(hole.get());
}

}  // namespace
}  // namespace chromeos
