// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/keymint/arc_keymint_bridge.h"

#include <gmock/gmock.h>

#include "chromeos/ash/experiences/arc/session/arc_bridge_service.h"
#include "chromeos/ash/experiences/arc/session/arc_service_manager.h"
#include "chromeos/ash/experiences/arc/test/connection_holder_util.h"
#include "chromeos/ash/experiences/arc/test/fake_keymint_instance.h"
#include "components/user_prefs/test/test_browser_context_with_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class MockCertStoreBridgeKeyMint : public keymint::CertStoreBridgeKeyMint {
 public:
  explicit MockCertStoreBridgeKeyMint(content::BrowserContext* context)
      : CertStoreBridgeKeyMint(context) {}
  MockCertStoreBridgeKeyMint(const MockCertStoreBridgeKeyMint&) = delete;
  MockCertStoreBridgeKeyMint& operator=(const MockCertStoreBridgeKeyMint&) =
      delete;
  ~MockCertStoreBridgeKeyMint() override = default;

  MOCK_METHOD1(SetSerialNumber, void(const std::string& serial_number));
};

class ArcKeyMintBridgeTest : public testing::Test {
 protected:
  ArcKeyMintBridgeTest() = default;
  ArcKeyMintBridgeTest(const ArcKeyMintBridgeTest&) = delete;
  ArcKeyMintBridgeTest& operator=(const ArcKeyMintBridgeTest&) = delete;
  ~ArcKeyMintBridgeTest() override = default;

  void SetUp() override {
    bridge_ = ArcKeyMintBridge::GetForBrowserContextForTesting(&context_);

    // Create the mock object.
    auto cert_store_bridge =
        std::make_unique<MockCertStoreBridgeKeyMint>(&context_);
    mock_cert_store_bridge_ = cert_store_bridge.get();
    bridge_->SetCertStoreBridgeForTesting(std::move(cert_store_bridge));

    EXPECT_EQ(keymint_instance()->num_init_called(), 0u);
    // This results in ArcKeyMintBridge::OnInstanceReady being called.
    ArcServiceManager::Get()->arc_bridge_service()->keymint()->SetInstance(
        &keymint_instance_);
    WaitForInstanceReady(
        ArcServiceManager::Get()->arc_bridge_service()->keymint());
    // Tests that KeyMintInstance's Init() method is called after the
    // instance connects to the host.
    EXPECT_EQ(keymint_instance()->num_init_called(), 1u);
  }

  ArcKeyMintBridge* bridge() { return bridge_; }
  const FakeKeyMintInstance* keymint_instance() const {
    return &keymint_instance_;
  }

  raw_ptr<keymint::CertStoreBridgeKeyMint> GetCertStoreBridge() {
    return cert_store_bridge_;
  }

  MockCertStoreBridgeKeyMint* mock_cert_store_bridge() {
    return mock_cert_store_bridge_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ArcServiceManager arc_service_manager_;
  FakeKeyMintInstance keymint_instance_;
  user_prefs::TestBrowserContextWithPrefs context_;
  raw_ptr<ArcKeyMintBridge> bridge_ = nullptr;
  raw_ptr<keymint::CertStoreBridgeKeyMint> cert_store_bridge_;
  raw_ptr<MockCertStoreBridgeKeyMint> mock_cert_store_bridge_;
};

TEST_F(ArcKeyMintBridgeTest, ConstructDestruct) {
  EXPECT_NE(bridge(), nullptr);
}

TEST_F(ArcKeyMintBridgeTest, SetSerialNumberSuccess) {
  // Prepare.
  std::string serial_number = "3124712407124vnf09r23";
  EXPECT_NE(mock_cert_store_bridge(), nullptr);
  EXPECT_CALL(*mock_cert_store_bridge(), SetSerialNumber(serial_number))
      .Times(1);

  // Execute.
  bridge()->SetSerialNumberInKeyMint(serial_number);
  bridge()->SendSerialNumberToKeyMintForTesting();
}

TEST_F(ArcKeyMintBridgeTest, SetSerialNumberFailure) {
  // Prepare.
  EXPECT_NE(mock_cert_store_bridge(), nullptr);
  EXPECT_CALL(*mock_cert_store_bridge(), SetSerialNumber(::testing::_))
      .Times(0);

  // Execute.
  bridge()->SendSerialNumberToKeyMintForTesting();
}

}  // namespace
}  // namespace arc
